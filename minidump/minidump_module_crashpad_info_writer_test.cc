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

#include <windows.h>
#include <dbghelp.h>

#include <utility>

#include "base/memory/ptr_util.h"
#include "gtest/gtest.h"
#include "minidump/minidump_extensions.h"
#include "minidump/minidump_simple_string_dictionary_writer.h"
#include "minidump/test/minidump_file_writer_test_util.h"
#include "minidump/test/minidump_string_writer_test_util.h"
#include "minidump/test/minidump_writable_test_util.h"
#include "snapshot/test/test_module_snapshot.h"
#include "util/file/string_file.h"

namespace crashpad {
namespace test {
namespace {

const MinidumpModuleCrashpadInfoList* MinidumpModuleCrashpadInfoListAtStart(
    const std::string& file_contents,
    size_t count) {
  MINIDUMP_LOCATION_DESCRIPTOR location_descriptor;
  location_descriptor.DataSize =
      static_cast<uint32_t>(sizeof(MinidumpModuleCrashpadInfoList) +
                            count * sizeof(MinidumpModuleCrashpadInfoLink));
  location_descriptor.Rva = 0;

  const MinidumpModuleCrashpadInfoList* list =
      MinidumpWritableAtLocationDescriptor<MinidumpModuleCrashpadInfoList>(
          file_contents, location_descriptor);
  if (!list) {
    return nullptr;
  }

  if (list->count != count) {
    EXPECT_EQ(list->count, count);
    return nullptr;
  }

  return list;
}

TEST(MinidumpModuleCrashpadInfoWriter, EmptyList) {
  StringFile string_file;

  auto module_list_writer =
      base::WrapUnique(new MinidumpModuleCrashpadInfoListWriter());
  EXPECT_FALSE(module_list_writer->IsUseful());

  EXPECT_TRUE(module_list_writer->WriteEverything(&string_file));
  ASSERT_EQ(string_file.string().size(),
            sizeof(MinidumpModuleCrashpadInfoList));

  const MinidumpModuleCrashpadInfoList* module_list =
      MinidumpModuleCrashpadInfoListAtStart(string_file.string(), 0);
  ASSERT_TRUE(module_list);
}

TEST(MinidumpModuleCrashpadInfoWriter, EmptyModule) {
  StringFile string_file;

  auto module_list_writer =
      base::WrapUnique(new MinidumpModuleCrashpadInfoListWriter());
  auto module_writer = base::WrapUnique(new MinidumpModuleCrashpadInfoWriter());
  EXPECT_FALSE(module_writer->IsUseful());
  module_list_writer->AddModule(std::move(module_writer), 0);

  EXPECT_TRUE(module_list_writer->IsUseful());

  EXPECT_TRUE(module_list_writer->WriteEverything(&string_file));
  ASSERT_EQ(string_file.string().size(),
            sizeof(MinidumpModuleCrashpadInfoList) +
                sizeof(MinidumpModuleCrashpadInfoLink) +
                sizeof(MinidumpModuleCrashpadInfo));

  const MinidumpModuleCrashpadInfoList* module_list =
      MinidumpModuleCrashpadInfoListAtStart(string_file.string(), 1);
  ASSERT_TRUE(module_list);

  EXPECT_EQ(module_list->modules[0].minidump_module_list_index, 0u);
  const MinidumpModuleCrashpadInfo* module =
      MinidumpWritableAtLocationDescriptor<MinidumpModuleCrashpadInfo>(
          string_file.string(), module_list->modules[0].location);
  ASSERT_TRUE(module);

  EXPECT_EQ(module->version, MinidumpModuleCrashpadInfo::kVersion);
  EXPECT_EQ(module->list_annotations.DataSize, 0u);
  EXPECT_EQ(module->list_annotations.Rva, 0u);
  EXPECT_EQ(module->simple_annotations.DataSize, 0u);
  EXPECT_EQ(module->simple_annotations.Rva, 0u);
}

TEST(MinidumpModuleCrashpadInfoWriter, FullModule) {
  constexpr uint32_t kMinidumpModuleListIndex = 1;
  static constexpr char kKey[] = "key";
  static constexpr char kValue[] = "value";
  static constexpr char kEntry[] = "entry";
  std::vector<std::string> vector(1, std::string(kEntry));

  StringFile string_file;

  auto module_list_writer =
      base::WrapUnique(new MinidumpModuleCrashpadInfoListWriter());

  auto module_writer = base::WrapUnique(new MinidumpModuleCrashpadInfoWriter());
  auto string_list_writer =
      base::WrapUnique(new MinidumpUTF8StringListWriter());
  string_list_writer->InitializeFromVector(vector);
  module_writer->SetListAnnotations(std::move(string_list_writer));
  auto simple_string_dictionary_writer =
      base::WrapUnique(new MinidumpSimpleStringDictionaryWriter());
  auto simple_string_dictionary_entry_writer =
      base::WrapUnique(new MinidumpSimpleStringDictionaryEntryWriter());
  simple_string_dictionary_entry_writer->SetKeyValue(kKey, kValue);
  simple_string_dictionary_writer->AddEntry(
      std::move(simple_string_dictionary_entry_writer));
  module_writer->SetSimpleAnnotations(
      std::move(simple_string_dictionary_writer));
  EXPECT_TRUE(module_writer->IsUseful());
  module_list_writer->AddModule(std::move(module_writer),
                                kMinidumpModuleListIndex);

  EXPECT_TRUE(module_list_writer->IsUseful());

  EXPECT_TRUE(module_list_writer->WriteEverything(&string_file));
  ASSERT_EQ(string_file.string().size(),
            sizeof(MinidumpModuleCrashpadInfoList) +
                sizeof(MinidumpModuleCrashpadInfoLink) +
                sizeof(MinidumpModuleCrashpadInfo) +
                sizeof(MinidumpRVAList) +
                sizeof(RVA) +
                sizeof(MinidumpSimpleStringDictionary) +
                sizeof(MinidumpSimpleStringDictionaryEntry) +
                sizeof(MinidumpUTF8String) + arraysize(kEntry) + 2 +  // padding
                sizeof(MinidumpUTF8String) + arraysize(kKey) +
                sizeof(MinidumpUTF8String) + arraysize(kValue));

  const MinidumpModuleCrashpadInfoList* module_list =
      MinidumpModuleCrashpadInfoListAtStart(string_file.string(), 1);
  ASSERT_TRUE(module_list);

  EXPECT_EQ(module_list->modules[0].minidump_module_list_index,
            kMinidumpModuleListIndex);
  const MinidumpModuleCrashpadInfo* module =
      MinidumpWritableAtLocationDescriptor<MinidumpModuleCrashpadInfo>(
          string_file.string(), module_list->modules[0].location);
  ASSERT_TRUE(module);

  EXPECT_EQ(module->version, MinidumpModuleCrashpadInfo::kVersion);
  EXPECT_NE(module->list_annotations.DataSize, 0u);
  EXPECT_NE(module->list_annotations.Rva, 0u);
  EXPECT_NE(module->simple_annotations.DataSize, 0u);
  EXPECT_NE(module->simple_annotations.Rva, 0u);

  const MinidumpRVAList* list_annotations =
      MinidumpWritableAtLocationDescriptor<MinidumpRVAList>(
          string_file.string(), module->list_annotations);
  ASSERT_TRUE(list_annotations);

  ASSERT_EQ(list_annotations->count, 1u);
  EXPECT_EQ(MinidumpUTF8StringAtRVAAsString(string_file.string(),
                                            list_annotations->children[0]),
            kEntry);

  const MinidumpSimpleStringDictionary* simple_annotations =
      MinidumpWritableAtLocationDescriptor<MinidumpSimpleStringDictionary>(
          string_file.string(), module->simple_annotations);
  ASSERT_TRUE(simple_annotations);

  ASSERT_EQ(simple_annotations->count, 1u);
  EXPECT_EQ(MinidumpUTF8StringAtRVAAsString(string_file.string(),
                                            simple_annotations->entries[0].key),
            kKey);
  EXPECT_EQ(MinidumpUTF8StringAtRVAAsString(
                string_file.string(), simple_annotations->entries[0].value),
            kValue);
}

TEST(MinidumpModuleCrashpadInfoWriter, ThreeModules) {
  constexpr uint32_t kMinidumpModuleListIndex0 = 0;
  static constexpr char kKey0[] = "key";
  static constexpr char kValue0[] = "value";
  constexpr uint32_t kMinidumpModuleListIndex1 = 2;
  constexpr uint32_t kMinidumpModuleListIndex2 = 5;
  static constexpr char kKey2A[] = "K";
  static constexpr char kValue2A[] = "VVV";
  static constexpr char kKey2B[] = "river";
  static constexpr char kValue2B[] = "hudson";

  StringFile string_file;

  auto module_list_writer =
      base::WrapUnique(new MinidumpModuleCrashpadInfoListWriter());

  auto module_writer_0 =
      base::WrapUnique(new MinidumpModuleCrashpadInfoWriter());
  auto simple_string_dictionary_writer_0 =
      base::WrapUnique(new MinidumpSimpleStringDictionaryWriter());
  auto simple_string_dictionary_entry_writer_0 =
      base::WrapUnique(new MinidumpSimpleStringDictionaryEntryWriter());
  simple_string_dictionary_entry_writer_0->SetKeyValue(kKey0, kValue0);
  simple_string_dictionary_writer_0->AddEntry(
      std::move(simple_string_dictionary_entry_writer_0));
  module_writer_0->SetSimpleAnnotations(
      std::move(simple_string_dictionary_writer_0));
  EXPECT_TRUE(module_writer_0->IsUseful());
  module_list_writer->AddModule(std::move(module_writer_0),
                                kMinidumpModuleListIndex0);

  auto module_writer_1 =
      base::WrapUnique(new MinidumpModuleCrashpadInfoWriter());
  EXPECT_FALSE(module_writer_1->IsUseful());
  module_list_writer->AddModule(std::move(module_writer_1),
                                kMinidumpModuleListIndex1);

  auto module_writer_2 =
      base::WrapUnique(new MinidumpModuleCrashpadInfoWriter());
  auto simple_string_dictionary_writer_2 =
      base::WrapUnique(new MinidumpSimpleStringDictionaryWriter());
  auto simple_string_dictionary_entry_writer_2a =
      base::WrapUnique(new MinidumpSimpleStringDictionaryEntryWriter());
  simple_string_dictionary_entry_writer_2a->SetKeyValue(kKey2A, kValue2A);
  simple_string_dictionary_writer_2->AddEntry(
      std::move(simple_string_dictionary_entry_writer_2a));
  auto simple_string_dictionary_entry_writer_2b =
      base::WrapUnique(new MinidumpSimpleStringDictionaryEntryWriter());
  simple_string_dictionary_entry_writer_2b->SetKeyValue(kKey2B, kValue2B);
  simple_string_dictionary_writer_2->AddEntry(
      std::move(simple_string_dictionary_entry_writer_2b));
  module_writer_2->SetSimpleAnnotations(
      std::move(simple_string_dictionary_writer_2));
  EXPECT_TRUE(module_writer_2->IsUseful());
  module_list_writer->AddModule(std::move(module_writer_2),
                                kMinidumpModuleListIndex2);

  EXPECT_TRUE(module_list_writer->IsUseful());

  EXPECT_TRUE(module_list_writer->WriteEverything(&string_file));

  const MinidumpModuleCrashpadInfoList* module_list =
      MinidumpModuleCrashpadInfoListAtStart(string_file.string(), 3);
  ASSERT_TRUE(module_list);

  EXPECT_EQ(module_list->modules[0].minidump_module_list_index,
            kMinidumpModuleListIndex0);
  const MinidumpModuleCrashpadInfo* module_0 =
      MinidumpWritableAtLocationDescriptor<MinidumpModuleCrashpadInfo>(
          string_file.string(), module_list->modules[0].location);
  ASSERT_TRUE(module_0);

  EXPECT_EQ(module_0->version, MinidumpModuleCrashpadInfo::kVersion);

  const MinidumpRVAList* list_annotations_0 =
      MinidumpWritableAtLocationDescriptor<MinidumpRVAList>(
          string_file.string(), module_0->list_annotations);
  EXPECT_FALSE(list_annotations_0);

  const MinidumpSimpleStringDictionary* simple_annotations_0 =
      MinidumpWritableAtLocationDescriptor<MinidumpSimpleStringDictionary>(
          string_file.string(), module_0->simple_annotations);
  ASSERT_TRUE(simple_annotations_0);

  ASSERT_EQ(simple_annotations_0->count, 1u);
  EXPECT_EQ(MinidumpUTF8StringAtRVAAsString(
                string_file.string(), simple_annotations_0->entries[0].key),
            kKey0);
  EXPECT_EQ(MinidumpUTF8StringAtRVAAsString(
                string_file.string(), simple_annotations_0->entries[0].value),
            kValue0);

  EXPECT_EQ(module_list->modules[1].minidump_module_list_index,
            kMinidumpModuleListIndex1);
  const MinidumpModuleCrashpadInfo* module_1 =
      MinidumpWritableAtLocationDescriptor<MinidumpModuleCrashpadInfo>(
          string_file.string(), module_list->modules[1].location);
  ASSERT_TRUE(module_1);

  EXPECT_EQ(module_1->version, MinidumpModuleCrashpadInfo::kVersion);

  const MinidumpRVAList* list_annotations_1 =
      MinidumpWritableAtLocationDescriptor<MinidumpRVAList>(
          string_file.string(), module_1->list_annotations);
  EXPECT_FALSE(list_annotations_1);

  const MinidumpSimpleStringDictionary* simple_annotations_1 =
      MinidumpWritableAtLocationDescriptor<MinidumpSimpleStringDictionary>(
          string_file.string(), module_1->simple_annotations);
  EXPECT_FALSE(simple_annotations_1);

  EXPECT_EQ(module_list->modules[2].minidump_module_list_index,
            kMinidumpModuleListIndex2);
  const MinidumpModuleCrashpadInfo* module_2 =
      MinidumpWritableAtLocationDescriptor<MinidumpModuleCrashpadInfo>(
          string_file.string(), module_list->modules[2].location);
  ASSERT_TRUE(module_2);

  EXPECT_EQ(module_2->version, MinidumpModuleCrashpadInfo::kVersion);

  const MinidumpRVAList* list_annotations_2 =
      MinidumpWritableAtLocationDescriptor<MinidumpRVAList>(
          string_file.string(), module_2->list_annotations);
  EXPECT_FALSE(list_annotations_2);

  const MinidumpSimpleStringDictionary* simple_annotations_2 =
      MinidumpWritableAtLocationDescriptor<MinidumpSimpleStringDictionary>(
          string_file.string(), module_2->simple_annotations);
  ASSERT_TRUE(simple_annotations_2);

  ASSERT_EQ(simple_annotations_2->count, 2u);
  EXPECT_EQ(MinidumpUTF8StringAtRVAAsString(
                string_file.string(), simple_annotations_2->entries[0].key),
            kKey2A);
  EXPECT_EQ(MinidumpUTF8StringAtRVAAsString(
                string_file.string(), simple_annotations_2->entries[0].value),
            kValue2A);
  EXPECT_EQ(MinidumpUTF8StringAtRVAAsString(
                string_file.string(), simple_annotations_2->entries[1].key),
            kKey2B);
  EXPECT_EQ(MinidumpUTF8StringAtRVAAsString(
                string_file.string(), simple_annotations_2->entries[1].value),
            kValue2B);
}

TEST(MinidumpModuleCrashpadInfoWriter, InitializeFromSnapshot) {
  static constexpr char kKey0A[] = "k";
  static constexpr char kValue0A[] = "value";
  static constexpr char kKey0B[] = "hudson";
  static constexpr char kValue0B[] = "estuary";
  static constexpr char kKey2[] = "k";
  static constexpr char kValue2[] = "different_value";
  static constexpr char kEntry3A[] = "list";
  static constexpr char kEntry3B[] = "erine";

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

  auto module_list_writer =
      base::WrapUnique(new MinidumpModuleCrashpadInfoListWriter());
  module_list_writer->InitializeFromSnapshot(module_snapshots);
  EXPECT_TRUE(module_list_writer->IsUseful());

  StringFile string_file;
  ASSERT_TRUE(module_list_writer->WriteEverything(&string_file));

  const MinidumpModuleCrashpadInfoList* module_list =
      MinidumpModuleCrashpadInfoListAtStart(string_file.string(), 3);
  ASSERT_TRUE(module_list);

  EXPECT_EQ(module_list->modules[0].minidump_module_list_index, 0u);
  const MinidumpModuleCrashpadInfo* module_0 =
      MinidumpWritableAtLocationDescriptor<MinidumpModuleCrashpadInfo>(
          string_file.string(), module_list->modules[0].location);
  ASSERT_TRUE(module_0);

  EXPECT_EQ(module_0->version, MinidumpModuleCrashpadInfo::kVersion);

  const MinidumpRVAList* list_annotations_0 =
      MinidumpWritableAtLocationDescriptor<MinidumpRVAList>(
          string_file.string(), module_0->list_annotations);
  EXPECT_FALSE(list_annotations_0);

  const MinidumpSimpleStringDictionary* simple_annotations_0 =
      MinidumpWritableAtLocationDescriptor<MinidumpSimpleStringDictionary>(
          string_file.string(), module_0->simple_annotations);
  ASSERT_TRUE(simple_annotations_0);

  ASSERT_EQ(simple_annotations_0->count, annotations_simple_map_0.size());
  EXPECT_EQ(MinidumpUTF8StringAtRVAAsString(
                string_file.string(), simple_annotations_0->entries[0].key),
            kKey0B);
  EXPECT_EQ(MinidumpUTF8StringAtRVAAsString(
                string_file.string(), simple_annotations_0->entries[0].value),
            kValue0B);
  EXPECT_EQ(MinidumpUTF8StringAtRVAAsString(
                string_file.string(), simple_annotations_0->entries[1].key),
            kKey0A);
  EXPECT_EQ(MinidumpUTF8StringAtRVAAsString(
                string_file.string(), simple_annotations_0->entries[1].value),
            kValue0A);

  EXPECT_EQ(module_list->modules[1].minidump_module_list_index, 2u);
  const MinidumpModuleCrashpadInfo* module_2 =
      MinidumpWritableAtLocationDescriptor<MinidumpModuleCrashpadInfo>(
          string_file.string(), module_list->modules[1].location);
  ASSERT_TRUE(module_2);

  EXPECT_EQ(module_2->version, MinidumpModuleCrashpadInfo::kVersion);

  const MinidumpRVAList* list_annotations_2 =
      MinidumpWritableAtLocationDescriptor<MinidumpRVAList>(
          string_file.string(), module_2->list_annotations);
  EXPECT_FALSE(list_annotations_2);

  const MinidumpSimpleStringDictionary* simple_annotations_2 =
      MinidumpWritableAtLocationDescriptor<MinidumpSimpleStringDictionary>(
          string_file.string(), module_2->simple_annotations);
  ASSERT_TRUE(simple_annotations_2);

  ASSERT_EQ(simple_annotations_2->count, annotations_simple_map_2.size());
  EXPECT_EQ(MinidumpUTF8StringAtRVAAsString(
                string_file.string(), simple_annotations_2->entries[0].key),
            kKey2);
  EXPECT_EQ(MinidumpUTF8StringAtRVAAsString(
                string_file.string(), simple_annotations_2->entries[0].value),
            kValue2);

  EXPECT_EQ(module_list->modules[2].minidump_module_list_index, 3u);
  const MinidumpModuleCrashpadInfo* module_3 =
      MinidumpWritableAtLocationDescriptor<MinidumpModuleCrashpadInfo>(
          string_file.string(), module_list->modules[2].location);
  ASSERT_TRUE(module_3);

  EXPECT_EQ(module_3->version, MinidumpModuleCrashpadInfo::kVersion);

  const MinidumpRVAList* list_annotations_3 =
      MinidumpWritableAtLocationDescriptor<MinidumpRVAList>(
          string_file.string(), module_3->list_annotations);
  ASSERT_TRUE(list_annotations_3);

  ASSERT_EQ(list_annotations_3->count, annotations_vector_3.size());
  EXPECT_EQ(MinidumpUTF8StringAtRVAAsString(string_file.string(),
                                            list_annotations_3->children[0]),
            kEntry3A);
  EXPECT_EQ(MinidumpUTF8StringAtRVAAsString(string_file.string(),
                                            list_annotations_3->children[1]),
            kEntry3B);

  const MinidumpSimpleStringDictionary* simple_annotations_3 =
      MinidumpWritableAtLocationDescriptor<MinidumpSimpleStringDictionary>(
          string_file.string(), module_3->simple_annotations);
  EXPECT_FALSE(simple_annotations_3);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
