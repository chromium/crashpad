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

#include "minidump/minidump_module_crashpad_info_writer.h"

#include <dbghelp.h>

#include "gtest/gtest.h"
#include "minidump/minidump_extensions.h"
#include "minidump/minidump_simple_string_dictionary_writer.h"
#include "minidump/test/minidump_file_writer_test_util.h"
#include "minidump/test/minidump_string_writer_test_util.h"
#include "minidump/test/minidump_writable_test_util.h"
#include "util/file/string_file_writer.h"

namespace crashpad {
namespace test {
namespace {

const MinidumpModuleCrashpadInfoList* MinidumpModuleCrashpadInfoListAtStart(
    const std::string& file_contents,
    size_t count) {
  MINIDUMP_LOCATION_DESCRIPTOR location_descriptor;
  location_descriptor.DataSize = sizeof(MinidumpModuleCrashpadInfoList) +
                                 count * sizeof(MINIDUMP_LOCATION_DESCRIPTOR);
  location_descriptor.Rva = 0;
  return MinidumpWritableAtLocationDescriptor<MinidumpModuleCrashpadInfoList>(
      file_contents, location_descriptor);
}

TEST(MinidumpModuleCrashpadInfoWriter, EmptyList) {
  StringFileWriter file_writer;

  MinidumpModuleCrashpadInfoListWriter module_list_writer;

  EXPECT_TRUE(module_list_writer.WriteEverything(&file_writer));
  ASSERT_EQ(sizeof(MinidumpModuleCrashpadInfoList),
            file_writer.string().size());

  const MinidumpModuleCrashpadInfoList* module_list =
      MinidumpModuleCrashpadInfoListAtStart(file_writer.string(), 0);
  ASSERT_TRUE(module_list);

  EXPECT_EQ(0u, module_list->count);
}

TEST(MinidumpModuleCrashpadInfoWriter, EmptyModule) {
  StringFileWriter file_writer;

  MinidumpModuleCrashpadInfoListWriter module_list_writer;
  MinidumpModuleCrashpadInfoWriter module_writer;
  module_list_writer.AddModule(&module_writer);

  EXPECT_TRUE(module_list_writer.WriteEverything(&file_writer));
  ASSERT_EQ(sizeof(MinidumpModuleCrashpadInfoList) +
                sizeof(MINIDUMP_LOCATION_DESCRIPTOR) +
                sizeof(MinidumpModuleCrashpadInfo),
            file_writer.string().size());

  const MinidumpModuleCrashpadInfoList* module_list =
      MinidumpModuleCrashpadInfoListAtStart(file_writer.string(), 1);
  ASSERT_TRUE(module_list);

  ASSERT_EQ(1u, module_list->count);

  const MinidumpModuleCrashpadInfo* module =
      MinidumpWritableAtLocationDescriptor<MinidumpModuleCrashpadInfo>(
          file_writer.string(), module_list->modules[0]);
  ASSERT_TRUE(module);

  EXPECT_EQ(MinidumpModuleCrashpadInfo::kVersion, module->version);
  EXPECT_EQ(0u, module->minidump_module_list_index);
  EXPECT_EQ(0u, module->simple_annotations.DataSize);
  EXPECT_EQ(0u, module->simple_annotations.Rva);
}

TEST(MinidumpModuleCrashpadInfoWriter, FullModule) {
  const uint32_t kMinidumpModuleListIndex = 1;
  const char kKey[] = "key";
  const char kValue[] = "value";

  StringFileWriter file_writer;

  MinidumpModuleCrashpadInfoListWriter module_list_writer;

  MinidumpModuleCrashpadInfoWriter module_writer;
  module_writer.SetMinidumpModuleListIndex(kMinidumpModuleListIndex);
  MinidumpSimpleStringDictionaryWriter simple_string_dictionary_writer;
  MinidumpSimpleStringDictionaryEntryWriter
      simple_string_dictionary_entry_writer;
  simple_string_dictionary_entry_writer.SetKeyValue(kKey, kValue);
  simple_string_dictionary_writer.AddEntry(
      &simple_string_dictionary_entry_writer);
  module_writer.SetSimpleAnnotations(&simple_string_dictionary_writer);
  module_list_writer.AddModule(&module_writer);

  EXPECT_TRUE(module_list_writer.WriteEverything(&file_writer));
  ASSERT_EQ(sizeof(MinidumpModuleCrashpadInfoList) +
                sizeof(MINIDUMP_LOCATION_DESCRIPTOR) +
                sizeof(MinidumpModuleCrashpadInfo) +
                sizeof(MinidumpSimpleStringDictionary) +
                sizeof(MinidumpSimpleStringDictionaryEntry) +
                sizeof(MinidumpUTF8String) + arraysize(kKey) +
                sizeof(MinidumpUTF8String) + arraysize(kValue),
            file_writer.string().size());

  const MinidumpModuleCrashpadInfoList* module_list =
      MinidumpModuleCrashpadInfoListAtStart(file_writer.string(), 1);
  ASSERT_TRUE(module_list);

  ASSERT_EQ(1u, module_list->count);

  const MinidumpModuleCrashpadInfo* module =
      MinidumpWritableAtLocationDescriptor<MinidumpModuleCrashpadInfo>(
          file_writer.string(), module_list->modules[0]);
  ASSERT_TRUE(module);

  EXPECT_EQ(MinidumpModuleCrashpadInfo::kVersion, module->version);
  EXPECT_EQ(kMinidumpModuleListIndex, module->minidump_module_list_index);
  EXPECT_NE(0u, module->simple_annotations.DataSize);
  EXPECT_NE(0u, module->simple_annotations.Rva);

  const MinidumpSimpleStringDictionary* simple_annotations =
      MinidumpWritableAtLocationDescriptor<MinidumpSimpleStringDictionary>(
          file_writer.string(), module->simple_annotations);
  ASSERT_TRUE(simple_annotations);

  ASSERT_EQ(1u, simple_annotations->count);
  EXPECT_EQ(kKey,
            MinidumpUTF8StringAtRVAAsString(
                file_writer.string(), simple_annotations->entries[0].key));
  EXPECT_EQ(kValue,
            MinidumpUTF8StringAtRVAAsString(
                file_writer.string(), simple_annotations->entries[0].value));
}

TEST(MinidumpModuleCrashpadInfoWriter, ThreeModules) {
  const uint32_t kMinidumpModuleListIndex0 = 0;
  const char kKey0[] = "key";
  const char kValue0[] = "value";
  const uint32_t kMinidumpModuleListIndex1 = 2;
  const uint32_t kMinidumpModuleListIndex2 = 5;
  const char kKey2A[] = "K";
  const char kValue2A[] = "VVV";
  const char kKey2B[] = "river";
  const char kValue2B[] = "hudson";

  StringFileWriter file_writer;

  MinidumpModuleCrashpadInfoListWriter module_list_writer;

  MinidumpModuleCrashpadInfoWriter module_writer_0;
  module_writer_0.SetMinidumpModuleListIndex(kMinidumpModuleListIndex0);
  MinidumpSimpleStringDictionaryWriter simple_string_dictionary_writer_0;
  MinidumpSimpleStringDictionaryEntryWriter
      simple_string_dictionary_entry_writer_0;
  simple_string_dictionary_entry_writer_0.SetKeyValue(kKey0, kValue0);
  simple_string_dictionary_writer_0.AddEntry(
      &simple_string_dictionary_entry_writer_0);
  module_writer_0.SetSimpleAnnotations(&simple_string_dictionary_writer_0);
  module_list_writer.AddModule(&module_writer_0);

  MinidumpModuleCrashpadInfoWriter module_writer_1;
  module_writer_1.SetMinidumpModuleListIndex(kMinidumpModuleListIndex1);
  module_list_writer.AddModule(&module_writer_1);

  MinidumpModuleCrashpadInfoWriter module_writer_2;
  module_writer_2.SetMinidumpModuleListIndex(kMinidumpModuleListIndex2);
  MinidumpSimpleStringDictionaryWriter simple_string_dictionary_writer_2;
  MinidumpSimpleStringDictionaryEntryWriter
      simple_string_dictionary_entry_writer_2a;
  simple_string_dictionary_entry_writer_2a.SetKeyValue(kKey2A, kValue2A);
  simple_string_dictionary_writer_2.AddEntry(
      &simple_string_dictionary_entry_writer_2a);
  MinidumpSimpleStringDictionaryEntryWriter
      simple_string_dictionary_entry_writer_2b;
  simple_string_dictionary_entry_writer_2b.SetKeyValue(kKey2B, kValue2B);
  simple_string_dictionary_writer_2.AddEntry(
      &simple_string_dictionary_entry_writer_2b);
  module_writer_2.SetSimpleAnnotations(&simple_string_dictionary_writer_2);
  module_list_writer.AddModule(&module_writer_2);

  EXPECT_TRUE(module_list_writer.WriteEverything(&file_writer));

  const MinidumpModuleCrashpadInfoList* module_list =
      MinidumpModuleCrashpadInfoListAtStart(file_writer.string(), 3);
  ASSERT_TRUE(module_list);

  ASSERT_EQ(3u, module_list->count);

  const MinidumpModuleCrashpadInfo* module_0 =
      MinidumpWritableAtLocationDescriptor<MinidumpModuleCrashpadInfo>(
          file_writer.string(), module_list->modules[0]);
  ASSERT_TRUE(module_0);

  EXPECT_EQ(MinidumpModuleCrashpadInfo::kVersion, module_0->version);
  EXPECT_EQ(kMinidumpModuleListIndex0, module_0->minidump_module_list_index);

  const MinidumpSimpleStringDictionary* simple_annotations_0 =
      MinidumpWritableAtLocationDescriptor<MinidumpSimpleStringDictionary>(
          file_writer.string(), module_0->simple_annotations);
  ASSERT_TRUE(simple_annotations_0);

  ASSERT_EQ(1u, simple_annotations_0->count);
  EXPECT_EQ(kKey0,
            MinidumpUTF8StringAtRVAAsString(
                file_writer.string(), simple_annotations_0->entries[0].key));
  EXPECT_EQ(kValue0,
            MinidumpUTF8StringAtRVAAsString(
                file_writer.string(), simple_annotations_0->entries[0].value));

  const MinidumpModuleCrashpadInfo* module_1 =
      MinidumpWritableAtLocationDescriptor<MinidumpModuleCrashpadInfo>(
          file_writer.string(), module_list->modules[1]);
  ASSERT_TRUE(module_1);

  EXPECT_EQ(MinidumpModuleCrashpadInfo::kVersion, module_1->version);
  EXPECT_EQ(kMinidumpModuleListIndex1, module_1->minidump_module_list_index);

  const MinidumpSimpleStringDictionary* simple_annotations_1 =
      MinidumpWritableAtLocationDescriptor<MinidumpSimpleStringDictionary>(
          file_writer.string(), module_1->simple_annotations);
  EXPECT_FALSE(simple_annotations_1);

  const MinidumpModuleCrashpadInfo* module_2 =
      MinidumpWritableAtLocationDescriptor<MinidumpModuleCrashpadInfo>(
          file_writer.string(), module_list->modules[2]);
  ASSERT_TRUE(module_2);

  EXPECT_EQ(MinidumpModuleCrashpadInfo::kVersion, module_2->version);
  EXPECT_EQ(kMinidumpModuleListIndex2, module_2->minidump_module_list_index);

  const MinidumpSimpleStringDictionary* simple_annotations_2 =
      MinidumpWritableAtLocationDescriptor<MinidumpSimpleStringDictionary>(
          file_writer.string(), module_2->simple_annotations);
  ASSERT_TRUE(simple_annotations_2);

  ASSERT_EQ(2u, simple_annotations_2->count);
  EXPECT_EQ(kKey2A,
            MinidumpUTF8StringAtRVAAsString(
                file_writer.string(), simple_annotations_2->entries[0].key));
  EXPECT_EQ(kValue2A,
            MinidumpUTF8StringAtRVAAsString(
                file_writer.string(), simple_annotations_2->entries[0].value));
  EXPECT_EQ(kKey2B,
            MinidumpUTF8StringAtRVAAsString(
                file_writer.string(), simple_annotations_2->entries[1].key));
  EXPECT_EQ(kValue2B,
            MinidumpUTF8StringAtRVAAsString(
                file_writer.string(), simple_annotations_2->entries[1].value));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
