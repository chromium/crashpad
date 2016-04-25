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

#include <utility>

#include "base/format_macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "gtest/gtest.h"
#include "minidump/minidump_rva_list_writer.h"
#include "minidump/test/minidump_rva_list_test_util.h"
#include "minidump/test/minidump_writable_test_util.h"
#include "util/file/string_file.h"

namespace crashpad {
namespace test {
namespace {

class TestMinidumpRVAListWriter final : public internal::MinidumpRVAListWriter {
 public:
  TestMinidumpRVAListWriter() : MinidumpRVAListWriter() {}
  ~TestMinidumpRVAListWriter() override {}

  void AddChild(uint32_t value) {
    auto child = base::WrapUnique(new TestUInt32MinidumpWritable(value));
    MinidumpRVAListWriter::AddChild(std::move(child));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestMinidumpRVAListWriter);
};

TEST(MinidumpRVAListWriter, Empty) {
  TestMinidumpRVAListWriter list_writer;

  StringFile string_file;

  ASSERT_TRUE(list_writer.WriteEverything(&string_file));
  EXPECT_EQ(sizeof(MinidumpRVAList), string_file.string().size());

  const MinidumpRVAList* list = MinidumpRVAListAtStart(string_file.string(), 0);
  ASSERT_TRUE(list);
}

TEST(MinidumpRVAListWriter, OneChild) {
  TestMinidumpRVAListWriter list_writer;

  const uint32_t kValue = 0;
  list_writer.AddChild(kValue);

  StringFile string_file;

  ASSERT_TRUE(list_writer.WriteEverything(&string_file));

  const MinidumpRVAList* list = MinidumpRVAListAtStart(string_file.string(), 1);
  ASSERT_TRUE(list);

  const uint32_t* child = MinidumpWritableAtRVA<uint32_t>(
      string_file.string(), list->children[0]);
  ASSERT_TRUE(child);
  EXPECT_EQ(kValue, *child);
}

TEST(MinidumpRVAListWriter, ThreeChildren) {
  TestMinidumpRVAListWriter list_writer;

  const uint32_t kValues[] = { 0x80000000, 0x55555555, 0x66006600 };

  list_writer.AddChild(kValues[0]);
  list_writer.AddChild(kValues[1]);
  list_writer.AddChild(kValues[2]);

  StringFile string_file;

  ASSERT_TRUE(list_writer.WriteEverything(&string_file));

  const MinidumpRVAList* list =
      MinidumpRVAListAtStart(string_file.string(), arraysize(kValues));
  ASSERT_TRUE(list);

  for (size_t index = 0; index < arraysize(kValues); ++index) {
    SCOPED_TRACE(base::StringPrintf("index %" PRIuS, index));

    const uint32_t* child = MinidumpWritableAtRVA<uint32_t>(
        string_file.string(), list->children[index]);
    ASSERT_TRUE(child);
    EXPECT_EQ(kValues[index], *child);
  }
}

}  // namespace
}  // namespace test
}  // namespace crashpad
