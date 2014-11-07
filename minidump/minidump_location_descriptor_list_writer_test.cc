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

#include "minidump/minidump_location_descriptor_list_writer.h"

#include "base/strings/stringprintf.h"
#include "gtest/gtest.h"
#include "minidump/test/minidump_location_descriptor_list_test_util.h"
#include "minidump/test/minidump_writable_test_util.h"
#include "util/file/string_file_writer.h"

namespace crashpad {
namespace test {
namespace {

class TestMinidumpLocationDescriptorListWriter final
    : public internal::MinidumpLocationDescriptorListWriter {
 public:
  TestMinidumpLocationDescriptorListWriter()
      : MinidumpLocationDescriptorListWriter() {
  }

  ~TestMinidumpLocationDescriptorListWriter() override {}

  void AddChild(uint32_t value) {
    auto child = make_scoped_ptr(new TestUInt32MinidumpWritable(value));
    MinidumpLocationDescriptorListWriter::AddChild(child.Pass());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestMinidumpLocationDescriptorListWriter);
};

TEST(MinidumpLocationDescriptorListWriter, Empty) {
  TestMinidumpLocationDescriptorListWriter list_writer;

  StringFileWriter file_writer;

  ASSERT_TRUE(list_writer.WriteEverything(&file_writer));
  EXPECT_EQ(sizeof(MinidumpLocationDescriptorList),
            file_writer.string().size());

  const MinidumpLocationDescriptorList* list =
      MinidumpLocationDescriptorListAtStart(file_writer.string(), 0);
  ASSERT_TRUE(list);
}

TEST(MinidumpLocationDescriptorListWriter, OneChild) {
  TestMinidumpLocationDescriptorListWriter list_writer;

  const uint32_t kValue = 0;
  list_writer.AddChild(kValue);

  StringFileWriter file_writer;

  ASSERT_TRUE(list_writer.WriteEverything(&file_writer));

  const MinidumpLocationDescriptorList* list =
      MinidumpLocationDescriptorListAtStart(file_writer.string(), 1);
  ASSERT_TRUE(list);

  const uint32_t* child = MinidumpWritableAtLocationDescriptor<uint32_t>(
      file_writer.string(), list->children[0]);
  ASSERT_TRUE(child);
  EXPECT_EQ(kValue, *child);
}

TEST(MinidumpLocationDescriptorListWriter, ThreeChildren) {
  TestMinidumpLocationDescriptorListWriter list_writer;

  const uint32_t kValues[] = { 0x80000000, 0x55555555, 0x66006600 };

  list_writer.AddChild(kValues[0]);
  list_writer.AddChild(kValues[1]);
  list_writer.AddChild(kValues[2]);

  StringFileWriter file_writer;

  ASSERT_TRUE(list_writer.WriteEverything(&file_writer));

  const MinidumpLocationDescriptorList* list =
      MinidumpLocationDescriptorListAtStart(file_writer.string(),
                                            arraysize(kValues));
  ASSERT_TRUE(list);

  for (size_t index = 0; index < arraysize(kValues); ++index) {
    SCOPED_TRACE(base::StringPrintf("index %zu", index));

    const uint32_t* child = MinidumpWritableAtLocationDescriptor<uint32_t>(
        file_writer.string(), list->children[index]);
    ASSERT_TRUE(child);
    EXPECT_EQ(kValues[index], *child);
  }
}

}  // namespace
}  // namespace test
}  // namespace crashpad
