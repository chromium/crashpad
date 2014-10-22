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

#include "minidump/minidump_crashpad_info_writer.h"

#include <dbghelp.h>

#include "gtest/gtest.h"
#include "minidump/minidump_extensions.h"
#include "minidump/minidump_file_writer.h"
#include "minidump/minidump_simple_string_dictionary_writer.h"
#include "minidump/test/minidump_file_writer_test_util.h"
#include "minidump/test/minidump_string_writer_test_util.h"
#include "minidump/test/minidump_writable_test_util.h"
#include "util/file/string_file_writer.h"

namespace crashpad {
namespace test {
namespace {

void GetCrashpadInfoStream(
    const std::string& file_contents,
    const MinidumpCrashpadInfo** crashpad_info,
    const MinidumpSimpleStringDictionary** simple_annotations) {
  const size_t kDirectoryOffset = sizeof(MINIDUMP_HEADER);
  const size_t kCrashpadInfoStreamOffset =
      kDirectoryOffset + sizeof(MINIDUMP_DIRECTORY);
  size_t end = kCrashpadInfoStreamOffset + sizeof(MinidumpCrashpadInfo);
  const size_t kSimpleAnnotationsOffset = simple_annotations ? end : 0;
  if (simple_annotations) {
    end += sizeof(MinidumpSimpleStringDictionary);
  }
  const size_t kFileSize = end;

  if (!simple_annotations) {
    ASSERT_EQ(kFileSize, file_contents.size());
  } else {
    EXPECT_GE(file_contents.size(), kFileSize);
  }

  const MINIDUMP_DIRECTORY* directory;
  const MINIDUMP_HEADER* header =
      MinidumpHeaderAtStart(file_contents, &directory);
  ASSERT_NO_FATAL_FAILURE(VerifyMinidumpHeader(header, 1, 0));
  ASSERT_TRUE(directory);

  ASSERT_EQ(kMinidumpStreamTypeCrashpadInfo, directory[0].StreamType);
  EXPECT_EQ(kCrashpadInfoStreamOffset, directory[0].Location.Rva);

  *crashpad_info = MinidumpWritableAtLocationDescriptor<MinidumpCrashpadInfo>(
      file_contents, directory[0].Location);
  ASSERT_TRUE(*crashpad_info);

  if (simple_annotations) {
    EXPECT_EQ(kSimpleAnnotationsOffset,
              (*crashpad_info)->simple_annotations.Rva);
    *simple_annotations =
        MinidumpWritableAtLocationDescriptor<MinidumpSimpleStringDictionary>(
            file_contents, (*crashpad_info)->simple_annotations);
    ASSERT_TRUE(*simple_annotations);
  } else {
    ASSERT_EQ(0u, (*crashpad_info)->simple_annotations.DataSize);
    ASSERT_EQ(0u, (*crashpad_info)->simple_annotations.Rva);
  }
}

TEST(MinidumpCrashpadInfoWriter, Empty) {
  MinidumpFileWriter minidump_file_writer;
  MinidumpCrashpadInfoWriter crashpad_info_writer;

  minidump_file_writer.AddStream(&crashpad_info_writer);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  const MinidumpCrashpadInfo* crashpad_info;

  ASSERT_NO_FATAL_FAILURE(
      GetCrashpadInfoStream(file_writer.string(), &crashpad_info, nullptr));

  EXPECT_EQ(sizeof(*crashpad_info), crashpad_info->size);
  EXPECT_EQ(1u, crashpad_info->version);
  EXPECT_EQ(0u, crashpad_info->simple_annotations.DataSize);
  EXPECT_EQ(0u, crashpad_info->simple_annotations.Rva);
}

TEST(MinidumpCrashpadInfoWriter, SimpleAnnotations) {
  MinidumpFileWriter minidump_file_writer;
  MinidumpCrashpadInfoWriter crashpad_info_writer;

  minidump_file_writer.AddStream(&crashpad_info_writer);

  MinidumpSimpleStringDictionaryWriter simple_annotations_writer;

  // Set a key and value before adding the simple_annotations_writer to
  // crashpad_info_writer, and another one after.
  const char kKey0[] = "k0";
  const char kValue0[] = "v";
  const char kKey1[] = "KEY1";
  const char kValue1[] = "";
  MinidumpSimpleStringDictionaryEntryWriter entry_0;
  entry_0.SetKeyValue(kKey0, kValue0);
  simple_annotations_writer.AddEntry(&entry_0);

  crashpad_info_writer.SetSimpleAnnotations(&simple_annotations_writer);

  MinidumpSimpleStringDictionaryEntryWriter entry_1;
  entry_1.SetKeyValue(kKey1, kValue1);
  simple_annotations_writer.AddEntry(&entry_1);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  const MinidumpCrashpadInfo* crashpad_info;
  const MinidumpSimpleStringDictionary* simple_annotations;

  ASSERT_NO_FATAL_FAILURE(GetCrashpadInfoStream(
      file_writer.string(), &crashpad_info, &simple_annotations));

  EXPECT_EQ(sizeof(*crashpad_info), crashpad_info->size);
  EXPECT_EQ(1u, crashpad_info->version);

  ASSERT_EQ(2u, simple_annotations->count);

  EXPECT_EQ(kKey1,
            MinidumpUTF8StringAtRVAAsString(
                file_writer.string(), simple_annotations->entries[0].key));
  EXPECT_EQ(kValue1,
            MinidumpUTF8StringAtRVAAsString(
                file_writer.string(), simple_annotations->entries[0].value));
  EXPECT_EQ(kKey0,
            MinidumpUTF8StringAtRVAAsString(
                file_writer.string(), simple_annotations->entries[1].key));
  EXPECT_EQ(kValue0,
            MinidumpUTF8StringAtRVAAsString(
                file_writer.string(), simple_annotations->entries[1].value));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
