// Copyright 2014 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "minidump/test/minidump_memory_writer_test_util.h"

#include "gtest/gtest.h"

namespace crashpad {
namespace test {

TestMinidumpMemoryWriter::TestMinidumpMemoryWriter(uint64_t base_address,
                                                   size_t size,
                                                   uint8_t value)
    : MinidumpMemoryWriter(),
      base_address_(base_address),
      expected_offset_(-1),
      size_(size),
      value_(value) {
}

TestMinidumpMemoryWriter::~TestMinidumpMemoryWriter() {
}

uint64_t TestMinidumpMemoryWriter::MemoryRangeBaseAddress() const {
  EXPECT_EQ(state(), kStateFrozen);
  return base_address_;
}

size_t TestMinidumpMemoryWriter::MemoryRangeSize() const {
  EXPECT_GE(state(), kStateFrozen);
  return size_;
}

bool TestMinidumpMemoryWriter::WillWriteAtOffsetImpl(FileOffset offset) {
  EXPECT_EQ(state(), kStateFrozen);
  expected_offset_ = offset;
  bool rv = MinidumpMemoryWriter::WillWriteAtOffsetImpl(offset);
  EXPECT_TRUE(rv);
  return rv;
}

bool TestMinidumpMemoryWriter::WriteObject(FileWriterInterface* file_writer) {
  EXPECT_EQ(state(), kStateWritable);
  EXPECT_EQ(expected_offset_, file_writer->Seek(0, SEEK_CUR));

  bool rv = true;
  if (size_ > 0) {
    std::string data(size_, value_);
    rv = file_writer->Write(&data[0], size_);
    EXPECT_TRUE(rv);
  }

  return rv;
}

void ExpectMinidumpMemoryDescriptor(
    const MINIDUMP_MEMORY_DESCRIPTOR* expected,
    const MINIDUMP_MEMORY_DESCRIPTOR* observed) {
  EXPECT_EQ(expected->StartOfMemoryRange, observed->StartOfMemoryRange);
  EXPECT_EQ(expected->Memory.DataSize, observed->Memory.DataSize);
  if (expected->Memory.Rva != 0) {
    const uint32_t kMemoryAlignment = 16;
    EXPECT_EQ(
        (expected->Memory.Rva + kMemoryAlignment - 1) & ~(kMemoryAlignment - 1),
        observed->Memory.Rva);
  }
}

void ExpectMinidumpMemoryDescriptorAndContents(
    const MINIDUMP_MEMORY_DESCRIPTOR* expected,
    const MINIDUMP_MEMORY_DESCRIPTOR* observed,
    const std::string& file_contents,
    uint8_t value,
    bool at_eof) {
  ExpectMinidumpMemoryDescriptor(expected, observed);

  if (at_eof) {
    EXPECT_EQ(file_contents.size(),
              observed->Memory.Rva + observed->Memory.DataSize);
  } else {
    EXPECT_GE(file_contents.size(),
              observed->Memory.Rva + observed->Memory.DataSize);
  }

  std::string expected_data(expected->Memory.DataSize, value);
  std::string observed_data(&file_contents[observed->Memory.Rva],
                            observed->Memory.DataSize);
  EXPECT_EQ(expected_data, observed_data);
}

}  // namespace test
}  // namespace crashpad
