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

#include "minidump/minidump_memory_writer.h"

#include <dbghelp.h>
#include <stdint.h>

#include "base/basictypes.h"
#include "gtest/gtest.h"
#include "minidump/minidump_extensions.h"
#include "minidump/minidump_file_writer.h"
#include "minidump/minidump_stream_writer.h"
#include "minidump/minidump_test_util.h"
#include "util/file/string_file_writer.h"

namespace {

using namespace crashpad;
using namespace crashpad::test;

const MinidumpStreamType kBogusStreamType =
    static_cast<MinidumpStreamType>(1234);

// expected_streams is the expected number of streams in the file. The memory
// list must be the last stream. If there is another stream, it must come first,
// have stream type kBogusStreamType, and have zero-length data.
void GetMemoryListStream(const std::string& file_contents,
                         const MINIDUMP_MEMORY_LIST** memory_list,
                         const uint32_t expected_streams) {
  const size_t kDirectoryOffset = sizeof(MINIDUMP_HEADER);
  const size_t kMemoryListStreamOffset =
      kDirectoryOffset + expected_streams * sizeof(MINIDUMP_DIRECTORY);
  const size_t kMemoryDescriptorsOffset =
      kMemoryListStreamOffset + sizeof(MINIDUMP_MEMORY_LIST);

  ASSERT_GE(file_contents.size(), kMemoryDescriptorsOffset);

  const MINIDUMP_HEADER* header =
      reinterpret_cast<const MINIDUMP_HEADER*>(&file_contents[0]);

  VerifyMinidumpHeader(header, expected_streams, 0);
  if (testing::Test::HasFatalFailure()) {
    return;
  }

  const MINIDUMP_DIRECTORY* directory =
      reinterpret_cast<const MINIDUMP_DIRECTORY*>(
          &file_contents[kDirectoryOffset]);

  if (expected_streams > 1) {
    ASSERT_EQ(kBogusStreamType, directory->StreamType);
    ASSERT_EQ(0u, directory->Location.DataSize);
    ASSERT_EQ(kMemoryListStreamOffset, directory->Location.Rva);
    ++directory;
  }

  ASSERT_EQ(kMinidumpStreamTypeMemoryList, directory->StreamType);
  ASSERT_GE(directory->Location.DataSize, sizeof(MINIDUMP_MEMORY_LIST));
  ASSERT_EQ(kMemoryListStreamOffset, directory->Location.Rva);

  *memory_list = reinterpret_cast<const MINIDUMP_MEMORY_LIST*>(
      &file_contents[kMemoryListStreamOffset]);

  ASSERT_EQ(sizeof(MINIDUMP_MEMORY_LIST) +
                (*memory_list)->NumberOfMemoryRanges *
                    sizeof(MINIDUMP_MEMORY_DESCRIPTOR),
            directory->Location.DataSize);
}

TEST(MinidumpMemoryWriter, EmptyMemoryList) {
  MinidumpFileWriter minidump_file_writer;
  MinidumpMemoryListWriter memory_list_writer;

  minidump_file_writer.AddStream(&memory_list_writer);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  ASSERT_EQ(sizeof(MINIDUMP_HEADER) + sizeof(MINIDUMP_DIRECTORY) +
                sizeof(MINIDUMP_MEMORY_LIST),
            file_writer.string().size());

  const MINIDUMP_MEMORY_LIST* memory_list;
  GetMemoryListStream(file_writer.string(), &memory_list, 1);
  if (Test::HasFatalFailure()) {
    return;
  }

  EXPECT_EQ(0u, memory_list->NumberOfMemoryRanges);
}

class TestMemoryWriter final : public MinidumpMemoryWriter {
 public:
  TestMemoryWriter(uint64_t base_address, size_t size, uint8_t value)
      : MinidumpMemoryWriter(),
        base_address_(base_address),
        expected_offset_(-1),
        size_(size),
        value_(value) {}

  ~TestMemoryWriter() {}

 protected:
  // MinidumpMemoryWriter:
  virtual uint64_t MemoryRangeBaseAddress() const override {
    EXPECT_EQ(state(), kStateFrozen);
    return base_address_;
  }

  virtual size_t MemoryRangeSize() const override {
    EXPECT_GE(state(), kStateFrozen);
    return size_;
  }

  // MinidumpWritable:
  virtual bool WillWriteAtOffsetImpl(off_t offset) override {
    EXPECT_EQ(state(), kStateFrozen);
    expected_offset_ = offset;
    bool rv = MinidumpMemoryWriter::WillWriteAtOffsetImpl(offset);
    EXPECT_TRUE(rv);
    return rv;
  }

  virtual bool WriteObject(FileWriterInterface* file_writer) override {
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

 private:
  uint64_t base_address_;
  off_t expected_offset_;
  size_t size_;
  uint8_t value_;

  DISALLOW_COPY_AND_ASSIGN(TestMemoryWriter);
};

void ExpectMemoryDescriptorAndContents(
    const MINIDUMP_MEMORY_DESCRIPTOR* expected,
    const MINIDUMP_MEMORY_DESCRIPTOR* observed,
    const std::string& file_contents,
    uint8_t value,
    bool at_eof) {
  const uint32_t kMemoryAlignment = 16;

  EXPECT_EQ(expected->StartOfMemoryRange, observed->StartOfMemoryRange);
  EXPECT_EQ(expected->Memory.DataSize, observed->Memory.DataSize);
  EXPECT_EQ(
      (expected->Memory.Rva + kMemoryAlignment - 1) & ~(kMemoryAlignment - 1),
      observed->Memory.Rva);
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

TEST(MinidumpMemoryWriter, OneMemoryRegion) {
  MinidumpFileWriter minidump_file_writer;
  MinidumpMemoryListWriter memory_list_writer;

  const uint64_t kBaseAddress = 0xfedcba9876543210ull;
  const uint64_t kSize = 0x1000;
  const uint8_t kValue = 'm';

  TestMemoryWriter memory_writer(kBaseAddress, kSize, kValue);
  memory_list_writer.AddMemory(&memory_writer);

  minidump_file_writer.AddStream(&memory_list_writer);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  const MINIDUMP_MEMORY_LIST* memory_list;
  GetMemoryListStream(file_writer.string(), &memory_list, 1);
  if (Test::HasFatalFailure()) {
    return;
  }

  MINIDUMP_MEMORY_DESCRIPTOR expected;
  expected.StartOfMemoryRange = kBaseAddress;
  expected.Memory.DataSize = kSize;
  expected.Memory.Rva =
      sizeof(MINIDUMP_HEADER) + sizeof(MINIDUMP_DIRECTORY) +
      sizeof(MINIDUMP_MEMORY_LIST) +
      memory_list->NumberOfMemoryRanges * sizeof(MINIDUMP_MEMORY_DESCRIPTOR);
  ExpectMemoryDescriptorAndContents(&expected,
                                    &memory_list->MemoryRanges[0],
                                    file_writer.string(),
                                    kValue,
                                    true);
}

TEST(MinidumpMemoryWriter, TwoMemoryRegions) {
  MinidumpFileWriter minidump_file_writer;
  MinidumpMemoryListWriter memory_list_writer;

  const uint64_t kBaseAddress1 = 0x00c0ffeeull;
  const uint64_t kSize1 = 0x0100;
  const uint8_t kValue1 = '6';
  const uint64_t kBaseAddress2 = 0xfac00facull;
  const uint64_t kSize2 = 0x0200;
  const uint8_t kValue2 = '!';

  TestMemoryWriter memory_writer_1(kBaseAddress1, kSize1, kValue1);
  memory_list_writer.AddMemory(&memory_writer_1);
  TestMemoryWriter memory_writer_2(kBaseAddress2, kSize2, kValue2);
  memory_list_writer.AddMemory(&memory_writer_2);

  minidump_file_writer.AddStream(&memory_list_writer);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  const MINIDUMP_MEMORY_LIST* memory_list;
  GetMemoryListStream(file_writer.string(), &memory_list, 1);
  if (Test::HasFatalFailure()) {
    return;
  }

  EXPECT_EQ(2u, memory_list->NumberOfMemoryRanges);

  MINIDUMP_MEMORY_DESCRIPTOR expected;

  {
    SCOPED_TRACE("region 0");

    expected.StartOfMemoryRange = kBaseAddress1;
    expected.Memory.DataSize = kSize1;
    expected.Memory.Rva =
        sizeof(MINIDUMP_HEADER) + sizeof(MINIDUMP_DIRECTORY) +
        sizeof(MINIDUMP_MEMORY_LIST) +
        memory_list->NumberOfMemoryRanges * sizeof(MINIDUMP_MEMORY_DESCRIPTOR);
    ExpectMemoryDescriptorAndContents(&expected,
                                      &memory_list->MemoryRanges[0],
                                      file_writer.string(),
                                      kValue1,
                                      false);
  }

  {
    SCOPED_TRACE("region 1");

    expected.StartOfMemoryRange = kBaseAddress2;
    expected.Memory.DataSize = kSize2;
    expected.Memory.Rva = memory_list->MemoryRanges[0].Memory.Rva +
                          memory_list->MemoryRanges[0].Memory.DataSize;
    ExpectMemoryDescriptorAndContents(&expected,
                                      &memory_list->MemoryRanges[1],
                                      file_writer.string(),
                                      kValue2,
                                      true);
  }
}

class TestMemoryStream final : public internal::MinidumpStreamWriter {
 public:
  TestMemoryStream(uint64_t base_address, size_t size, uint8_t value)
      : MinidumpStreamWriter(), memory_(base_address, size, value) {}

  ~TestMemoryStream() {}

  TestMemoryWriter* memory() { return &memory_; }

  // MinidumpStreamWriter:
  virtual MinidumpStreamType StreamType() const override {
    return kBogusStreamType;
  }

 protected:
  // MinidumpWritable:
  virtual size_t SizeOfObject() override {
    EXPECT_GE(state(), kStateFrozen);
    return 0;
  }

  virtual std::vector<MinidumpWritable*> Children() override {
    EXPECT_GE(state(), kStateFrozen);
    std::vector<MinidumpWritable*> children(1, memory());
    return children;
  }

  virtual bool WriteObject(FileWriterInterface* file_writer) override {
    EXPECT_EQ(kStateWritable, state());
    return true;
  }

 private:
  TestMemoryWriter memory_;

  DISALLOW_COPY_AND_ASSIGN(TestMemoryStream);
};

TEST(MinidumpMemoryWriter, ExtraMemory) {
  // This tests MinidumpMemoryListWriter::AddExtraMemory(). That method adds
  // a MinidumpMemoryWriter to the MinidumpMemoryListWriter without making the
  // memory writer a child of the memory list writer.
  MinidumpFileWriter minidump_file_writer;

  const uint64_t kBaseAddress1 = 0x0000000000001000ull;
  const uint64_t kSize1 = 0x0400;
  const uint8_t kValue1 = '1';
  TestMemoryStream test_memory_stream(kBaseAddress1, kSize1, kValue1);

  MinidumpMemoryListWriter memory_list_writer;
  memory_list_writer.AddExtraMemory(test_memory_stream.memory());

  minidump_file_writer.AddStream(&test_memory_stream);

  const uint64_t kBaseAddress2 = 0x0000000000002000ull;
  const uint64_t kSize2 = 0x0400;
  const uint8_t kValue2 = 'm';

  TestMemoryWriter memory_writer(kBaseAddress2, kSize2, kValue2);
  memory_list_writer.AddMemory(&memory_writer);

  minidump_file_writer.AddStream(&memory_list_writer);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  const MINIDUMP_MEMORY_LIST* memory_list;
  GetMemoryListStream(file_writer.string(), &memory_list, 2);
  if (Test::HasFatalFailure()) {
    return;
  }

  EXPECT_EQ(2u, memory_list->NumberOfMemoryRanges);

  MINIDUMP_MEMORY_DESCRIPTOR expected;

  {
    SCOPED_TRACE("region 0");

    expected.StartOfMemoryRange = kBaseAddress1;
    expected.Memory.DataSize = kSize1;
    expected.Memory.Rva =
        sizeof(MINIDUMP_HEADER) + 2 * sizeof(MINIDUMP_DIRECTORY) +
        sizeof(MINIDUMP_MEMORY_LIST) +
        memory_list->NumberOfMemoryRanges * sizeof(MINIDUMP_MEMORY_DESCRIPTOR);
    ExpectMemoryDescriptorAndContents(&expected,
                                      &memory_list->MemoryRanges[0],
                                      file_writer.string(),
                                      kValue1,
                                      false);
  }

  {
    SCOPED_TRACE("region 1");

    expected.StartOfMemoryRange = kBaseAddress2;
    expected.Memory.DataSize = kSize2;
    expected.Memory.Rva = memory_list->MemoryRanges[0].Memory.Rva +
                          memory_list->MemoryRanges[0].Memory.DataSize;
    ExpectMemoryDescriptorAndContents(&expected,
                                      &memory_list->MemoryRanges[1],
                                      file_writer.string(),
                                      kValue2,
                                      true);
  }
}

}  // namespace
