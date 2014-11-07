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
#include "minidump/test/minidump_location_descriptor_list_test_util.h"
#include "minidump/test/minidump_string_writer_test_util.h"
#include "minidump/test/minidump_writable_test_util.h"
#include "snapshot/test/test_module_snapshot.h"
#include "util/file/string_file_writer.h"

namespace crashpad {
namespace test {
namespace {

TEST(MinidumpModuleCrashpadInfoWriter, EmptyList) {
  StringFileWriter file_writer;

  MinidumpModuleCrashpadInfoListWriter module_list_writer;
  EXPECT_FALSE(module_list_writer.IsUseful());

  EXPECT_TRUE(module_list_writer.WriteEverything(&file_writer));
  ASSERT_EQ(sizeof(MinidumpModuleCrashpadInfoList),
            file_writer.string().size());

  const MinidumpModuleCrashpadInfoList* module_list =
      MinidumpLocationDescriptorListAtStart(file_writer.string(), 0);
  ASSERT_TRUE(module_list);
}

TEST(MinidumpModuleCrashpadInfoWriter, EmptyModule) {
  StringFileWriter file_writer;

  MinidumpModuleCrashpadInfoListWriter module_list_writer;
  auto module_writer =
      make_scoped_ptr(new MinidumpModuleCrashpadInfoWriter());
  EXPECT_FALSE(module_writer->IsUseful());
  module_list_writer.AddModule(module_writer.Pass());

  EXPECT_TRUE(module_list_writer.IsUseful());

  EXPECT_TRUE(module_list_writer.WriteEverything(&file_writer));
  ASSERT_EQ(sizeof(MinidumpModuleCrashpadInfoList) +
                sizeof(MINIDUMP_LOCATION_DESCRIPTOR) +
                sizeof(MinidumpModuleCrashpadInfo),
            file_writer.string().size());

  const MinidumpModuleCrashpadInfoList* module_list =
      MinidumpLocationDescriptorListAtStart(file_writer.string(), 1);
  ASSERT_TRUE(module_list);

  const MinidumpModuleCrashpadInfo* module =
      MinidumpWritableAtLocationDescriptor<MinidumpModuleCrashpadInfo>(
          file_writer.string(), module_list->children[0]);
  ASSERT_TRUE(module);

  EXPECT_EQ(MinidumpModuleCrashpadInfo::kVersion, module->version);
  EXPECT_EQ(0u, module->minidump_module_list_index);
  EXPECT_EQ(0u, module->list_annotations.DataSize);
  EXPECT_EQ(0u, module->list_annotations.Rva);
  EXPECT_EQ(0u, module->simple_annotations.DataSize);
  EXPECT_EQ(0u, module->simple_annotations.Rva);
}

TEST(MinidumpModuleCrashpadInfoWriter, FullModule) {
  const uint32_t kMinidumpModuleListIndex = 1;
  const char kKey[] = "key";
  const char kValue[] = "value";
  const char kEntry[] = "entry";
  std::vector<std::string> vector(1, std::string(kEntry));

  StringFileWriter file_writer;

  MinidumpModuleCrashpadInfoListWriter module_list_writer;

  auto module_writer =
      make_scoped_ptr(new MinidumpModuleCrashpadInfoWriter());
  module_writer->SetMinidumpModuleListIndex(kMinidumpModuleListIndex);
  auto string_list_writer = make_scoped_ptr(new MinidumpUTF8StringListWriter());
  string_list_writer->InitializeFromVector(vector);
  module_writer->SetListAnnotations(string_list_writer.Pass());
  auto simple_string_dictionary_writer =
      make_scoped_ptr(new MinidumpSimpleStringDictionaryWriter());
  auto simple_string_dictionary_entry_writer =
      make_scoped_ptr(new MinidumpSimpleStringDictionaryEntryWriter());
  simple_string_dictionary_entry_writer->SetKeyValue(kKey, kValue);
  simple_string_dictionary_writer->AddEntry(
      simple_string_dictionary_entry_writer.Pass());
  module_writer->SetSimpleAnnotations(simple_string_dictionary_writer.Pass());
  EXPECT_TRUE(module_writer->IsUseful());
  module_list_writer.AddModule(module_writer.Pass());

  EXPECT_TRUE(module_list_writer.IsUseful());

  EXPECT_TRUE(module_list_writer.WriteEverything(&file_writer));
  ASSERT_EQ(sizeof(MinidumpModuleCrashpadInfoList) +
                sizeof(MINIDUMP_LOCATION_DESCRIPTOR) +
                sizeof(MinidumpModuleCrashpadInfo) +
                sizeof(MinidumpRVAList) +
                sizeof(RVA) +
                sizeof(MinidumpSimpleStringDictionary) +
                sizeof(MinidumpSimpleStringDictionaryEntry) +
                sizeof(MinidumpUTF8String) + arraysize(kEntry) + 2 +  // padding
                sizeof(MinidumpUTF8String) + arraysize(kKey) +
                sizeof(MinidumpUTF8String) + arraysize(kValue),
            file_writer.string().size());

  const MinidumpModuleCrashpadInfoList* module_list =
      MinidumpLocationDescriptorListAtStart(file_writer.string(), 1);
  ASSERT_TRUE(module_list);

  const MinidumpModuleCrashpadInfo* module =
      MinidumpWritableAtLocationDescriptor<MinidumpModuleCrashpadInfo>(
          file_writer.string(), module_list->children[0]);
  ASSERT_TRUE(module);

  EXPECT_EQ(MinidumpModuleCrashpadInfo::kVersion, module->version);
  EXPECT_EQ(kMinidumpModuleListIndex, module->minidump_module_list_index);
  EXPECT_NE(0u, module->list_annotations.DataSize);
  EXPECT_NE(0u, module->list_annotations.Rva);
  EXPECT_NE(0u, module->simple_annotations.DataSize);
  EXPECT_NE(0u, module->simple_annotations.Rva);

  const MinidumpRVAList* list_annotations =
      MinidumpWritableAtLocationDescriptor<MinidumpRVAList>(
          file_writer.string(), module->list_annotations);
  ASSERT_TRUE(list_annotations);

  ASSERT_EQ(1u, list_annotations->count);
  EXPECT_EQ(kEntry, MinidumpUTF8StringAtRVAAsString(
      file_writer.string(), list_annotations->children[0]));

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

  auto module_writer_0 =
      make_scoped_ptr(new MinidumpModuleCrashpadInfoWriter());
  module_writer_0->SetMinidumpModuleListIndex(kMinidumpModuleListIndex0);
  auto simple_string_dictionary_writer_0 =
      make_scoped_ptr(new MinidumpSimpleStringDictionaryWriter());
  auto simple_string_dictionary_entry_writer_0 =
      make_scoped_ptr(new MinidumpSimpleStringDictionaryEntryWriter());
  simple_string_dictionary_entry_writer_0->SetKeyValue(kKey0, kValue0);
  simple_string_dictionary_writer_0->AddEntry(
      simple_string_dictionary_entry_writer_0.Pass());
  module_writer_0->SetSimpleAnnotations(
      simple_string_dictionary_writer_0.Pass());
  EXPECT_TRUE(module_writer_0->IsUseful());
  module_list_writer.AddModule(module_writer_0.Pass());

  auto module_writer_1 =
      make_scoped_ptr(new MinidumpModuleCrashpadInfoWriter());
  module_writer_1->SetMinidumpModuleListIndex(kMinidumpModuleListIndex1);
  EXPECT_FALSE(module_writer_1->IsUseful());
  module_list_writer.AddModule(module_writer_1.Pass());

  auto module_writer_2 =
      make_scoped_ptr(new MinidumpModuleCrashpadInfoWriter());
  module_writer_2->SetMinidumpModuleListIndex(kMinidumpModuleListIndex2);
  auto simple_string_dictionary_writer_2 =
      make_scoped_ptr(new MinidumpSimpleStringDictionaryWriter());
  auto simple_string_dictionary_entry_writer_2a =
      make_scoped_ptr(new MinidumpSimpleStringDictionaryEntryWriter());
  simple_string_dictionary_entry_writer_2a->SetKeyValue(kKey2A, kValue2A);
  simple_string_dictionary_writer_2->AddEntry(
      simple_string_dictionary_entry_writer_2a.Pass());
  auto simple_string_dictionary_entry_writer_2b =
      make_scoped_ptr(new MinidumpSimpleStringDictionaryEntryWriter());
  simple_string_dictionary_entry_writer_2b->SetKeyValue(kKey2B, kValue2B);
  simple_string_dictionary_writer_2->AddEntry(
      simple_string_dictionary_entry_writer_2b.Pass());
  module_writer_2->SetSimpleAnnotations(
      simple_string_dictionary_writer_2.Pass());
  EXPECT_TRUE(module_writer_2->IsUseful());
  module_list_writer.AddModule(module_writer_2.Pass());

  EXPECT_TRUE(module_list_writer.IsUseful());

  EXPECT_TRUE(module_list_writer.WriteEverything(&file_writer));

  const MinidumpModuleCrashpadInfoList* module_list =
      MinidumpLocationDescriptorListAtStart(file_writer.string(), 3);
  ASSERT_TRUE(module_list);

  const MinidumpModuleCrashpadInfo* module_0 =
      MinidumpWritableAtLocationDescriptor<MinidumpModuleCrashpadInfo>(
          file_writer.string(), module_list->children[0]);
  ASSERT_TRUE(module_0);

  EXPECT_EQ(MinidumpModuleCrashpadInfo::kVersion, module_0->version);
  EXPECT_EQ(kMinidumpModuleListIndex0, module_0->minidump_module_list_index);

  const MinidumpRVAList* list_annotations_0 =
      MinidumpWritableAtLocationDescriptor<MinidumpRVAList>(
          file_writer.string(), module_0->list_annotations);
  EXPECT_FALSE(list_annotations_0);

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
          file_writer.string(), module_list->children[1]);
  ASSERT_TRUE(module_1);

  EXPECT_EQ(MinidumpModuleCrashpadInfo::kVersion, module_1->version);
  EXPECT_EQ(kMinidumpModuleListIndex1, module_1->minidump_module_list_index);

  const MinidumpRVAList* list_annotations_1 =
      MinidumpWritableAtLocationDescriptor<MinidumpRVAList>(
          file_writer.string(), module_1->list_annotations);
  EXPECT_FALSE(list_annotations_1);

  const MinidumpSimpleStringDictionary* simple_annotations_1 =
      MinidumpWritableAtLocationDescriptor<MinidumpSimpleStringDictionary>(
          file_writer.string(), module_1->simple_annotations);
  EXPECT_FALSE(simple_annotations_1);

  const MinidumpModuleCrashpadInfo* module_2 =
      MinidumpWritableAtLocationDescriptor<MinidumpModuleCrashpadInfo>(
          file_writer.string(), module_list->children[2]);
  ASSERT_TRUE(module_2);

  EXPECT_EQ(MinidumpModuleCrashpadInfo::kVersion, module_2->version);
  EXPECT_EQ(kMinidumpModuleListIndex2, module_2->minidump_module_list_index);

  const MinidumpRVAList* list_annotations_2 =
      MinidumpWritableAtLocationDescriptor<MinidumpRVAList>(
          file_writer.string(), module_2->list_annotations);
  EXPECT_FALSE(list_annotations_2);

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

TEST(MinidumpModuleCrashpadInfoWriter, InitializeFromSnapshot) {
  const char kKey0A[] = "k";
  const char kValue0A[] = "value";
  const char kKey0B[] = "hudson";
  const char kValue0B[] = "estuary";
  const char kKey2[] = "k";
  const char kValue2[] = "different_value";
  const char kEntry3A[] = "list";
  const char kEntry3B[] = "erine";

  std::vector<const ModuleSnapshot*> module_snapshots;

  TestModuleSnapshot module_snapshot_0;
  std::map<std::string, std::string> annotations_simple_map_0;
  annotations_simple_map_0[kKey0A] = kValue0A;
  annotations_simple_map_0[kKey0B] = kValue0B;
  module_snapshot_0.SetAnnotationsSimpleMap(annotations_simple_map_0);
  module_snapshots.push_back(&module_snapshot_0);

  // module_snapshot_1 is not expected to be written because it would not carry
  // any MinidumpModuleCrashpadInfo data.
  TestModuleSnapshot module_snapshot_1;
  module_snapshots.push_back(&module_snapshot_1);

  TestModuleSnapshot module_snapshot_2;
  std::map<std::string, std::string> annotations_simple_map_2;
  annotations_simple_map_2[kKey2] = kValue2;
  module_snapshot_2.SetAnnotationsSimpleMap(annotations_simple_map_2);
  module_snapshots.push_back(&module_snapshot_2);

  TestModuleSnapshot module_snapshot_3;
  std::vector<std::string> annotations_vector_3;
  annotations_vector_3.push_back(kEntry3A);
  annotations_vector_3.push_back(kEntry3B);
  module_snapshot_3.SetAnnotationsVector(annotations_vector_3);
  module_snapshots.push_back(&module_snapshot_3);

  MinidumpModuleCrashpadInfoListWriter module_list_writer;
  module_list_writer.InitializeFromSnapshot(module_snapshots);
  EXPECT_TRUE(module_list_writer.IsUseful());

  StringFileWriter file_writer;
  ASSERT_TRUE(module_list_writer.WriteEverything(&file_writer));

  const MinidumpModuleCrashpadInfoList* module_list =
      MinidumpLocationDescriptorListAtStart(file_writer.string(), 3);
  ASSERT_TRUE(module_list);

  const MinidumpModuleCrashpadInfo* module_0 =
      MinidumpWritableAtLocationDescriptor<MinidumpModuleCrashpadInfo>(
          file_writer.string(), module_list->children[0]);
  ASSERT_TRUE(module_0);

  EXPECT_EQ(MinidumpModuleCrashpadInfo::kVersion, module_0->version);
  EXPECT_EQ(0u, module_0->minidump_module_list_index);

  const MinidumpRVAList* list_annotations_0 =
      MinidumpWritableAtLocationDescriptor<MinidumpRVAList>(
          file_writer.string(), module_0->list_annotations);
  EXPECT_FALSE(list_annotations_0);

  const MinidumpSimpleStringDictionary* simple_annotations_0 =
      MinidumpWritableAtLocationDescriptor<MinidumpSimpleStringDictionary>(
          file_writer.string(), module_0->simple_annotations);
  ASSERT_TRUE(simple_annotations_0);

  ASSERT_EQ(annotations_simple_map_0.size(), simple_annotations_0->count);
  EXPECT_EQ(kKey0B,
            MinidumpUTF8StringAtRVAAsString(
                file_writer.string(), simple_annotations_0->entries[0].key));
  EXPECT_EQ(kValue0B,
            MinidumpUTF8StringAtRVAAsString(
                file_writer.string(), simple_annotations_0->entries[0].value));
  EXPECT_EQ(kKey0A,
            MinidumpUTF8StringAtRVAAsString(
                file_writer.string(), simple_annotations_0->entries[1].key));
  EXPECT_EQ(kValue0A,
            MinidumpUTF8StringAtRVAAsString(
                file_writer.string(), simple_annotations_0->entries[1].value));

  const MinidumpModuleCrashpadInfo* module_2 =
      MinidumpWritableAtLocationDescriptor<MinidumpModuleCrashpadInfo>(
          file_writer.string(), module_list->children[1]);
  ASSERT_TRUE(module_2);

  EXPECT_EQ(MinidumpModuleCrashpadInfo::kVersion, module_2->version);
  EXPECT_EQ(2u, module_2->minidump_module_list_index);

  const MinidumpRVAList* list_annotations_2 =
      MinidumpWritableAtLocationDescriptor<MinidumpRVAList>(
          file_writer.string(), module_2->list_annotations);
  EXPECT_FALSE(list_annotations_2);

  const MinidumpSimpleStringDictionary* simple_annotations_2 =
      MinidumpWritableAtLocationDescriptor<MinidumpSimpleStringDictionary>(
          file_writer.string(), module_2->simple_annotations);
  ASSERT_TRUE(simple_annotations_2);

  ASSERT_EQ(annotations_simple_map_2.size(), simple_annotations_2->count);
  EXPECT_EQ(kKey2,
            MinidumpUTF8StringAtRVAAsString(
                file_writer.string(), simple_annotations_2->entries[0].key));
  EXPECT_EQ(kValue2,
            MinidumpUTF8StringAtRVAAsString(
                file_writer.string(), simple_annotations_2->entries[0].value));

  const MinidumpModuleCrashpadInfo* module_3 =
      MinidumpWritableAtLocationDescriptor<MinidumpModuleCrashpadInfo>(
          file_writer.string(), module_list->children[2]);
  ASSERT_TRUE(module_3);

  EXPECT_EQ(MinidumpModuleCrashpadInfo::kVersion, module_3->version);
  EXPECT_EQ(3u, module_3->minidump_module_list_index);

  const MinidumpRVAList* list_annotations_3 =
      MinidumpWritableAtLocationDescriptor<MinidumpRVAList>(
          file_writer.string(), module_3->list_annotations);
  ASSERT_TRUE(list_annotations_3);

  ASSERT_EQ(annotations_vector_3.size(), list_annotations_3->count);
  EXPECT_EQ(kEntry3A,
            MinidumpUTF8StringAtRVAAsString(
                file_writer.string(), list_annotations_3->children[0]));
  EXPECT_EQ(kEntry3B,
            MinidumpUTF8StringAtRVAAsString(
                file_writer.string(), list_annotations_3->children[1]));

  const MinidumpSimpleStringDictionary* simple_annotations_3 =
      MinidumpWritableAtLocationDescriptor<MinidumpSimpleStringDictionary>(
          file_writer.string(), module_3->simple_annotations);
  EXPECT_FALSE(simple_annotations_3);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
