// Copyright 2015 The Crashpad Authors. All rights reserved.
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

#include "snapshot/minidump/process_snapshot_minidump.h"

#include <windows.h>
#include <dbghelp.h>
#include <string.h>

#include "base/memory/scoped_ptr.h"
#include "gtest/gtest.h"
#include "snapshot/module_snapshot.h"
#include "util/file/string_file.h"

namespace crashpad {
namespace test {
namespace {

TEST(ProcessSnapshotMinidump, EmptyFile) {
  StringFile string_file;
  ProcessSnapshotMinidump process_snapshot;

  EXPECT_FALSE(process_snapshot.Initialize(&string_file));
}

TEST(ProcessSnapshotMinidump, InvalidSignatureAndVersion) {
  StringFile string_file;

  MINIDUMP_HEADER header = {};

  EXPECT_TRUE(string_file.Write(&header, sizeof(header)));

  ProcessSnapshotMinidump process_snapshot;
  EXPECT_FALSE(process_snapshot.Initialize(&string_file));
}

TEST(ProcessSnapshotMinidump, Empty) {
  StringFile string_file;

  MINIDUMP_HEADER header = {};
  header.Signature = MINIDUMP_SIGNATURE;
  header.Version = MINIDUMP_VERSION;

  EXPECT_TRUE(string_file.Write(&header, sizeof(header)));

  ProcessSnapshotMinidump process_snapshot;
  EXPECT_TRUE(process_snapshot.Initialize(&string_file));

  UUID client_id;
  process_snapshot.ClientID(&client_id);
  EXPECT_EQ(UUID(), client_id);

  EXPECT_TRUE(process_snapshot.AnnotationsSimpleMap().empty());
}

// Writes |string| to |writer| as a MinidumpUTF8String, and returns the file
// offst of the beginning of the string.
RVA WriteString(FileWriterInterface* writer, const std::string& string) {
  RVA rva = static_cast<RVA>(writer->SeekGet());

  uint32_t string_size = static_cast<uint32_t>(string.size());
  EXPECT_TRUE(writer->Write(&string_size, sizeof(string_size)));

  // Include the trailing NUL character.
  EXPECT_TRUE(writer->Write(string.c_str(), string.size() + 1));

  return rva;
}

// Writes |dictionary| to |writer| as a MinidumpSimpleStringDictionary, and
// populates |location| with a location descriptor identifying what was written.
void WriteMinidumpSimpleStringDictionary(
    MINIDUMP_LOCATION_DESCRIPTOR* location,
    FileWriterInterface* writer,
    const std::map<std::string, std::string>& dictionary) {
  std::vector<MinidumpSimpleStringDictionaryEntry> entries;
  for (const auto& it : dictionary) {
    MinidumpSimpleStringDictionaryEntry entry;
    entry.key = WriteString(writer, it.first);
    entry.value = WriteString(writer, it.second);
    entries.push_back(entry);
  }

  location->Rva = static_cast<RVA>(writer->SeekGet());

  const uint32_t simple_string_dictionary_entries =
      static_cast<uint32_t>(entries.size());
  EXPECT_TRUE(writer->Write(&simple_string_dictionary_entries,
                            sizeof(simple_string_dictionary_entries)));
  for (const MinidumpSimpleStringDictionaryEntry& entry : entries) {
    EXPECT_TRUE(writer->Write(&entry, sizeof(entry)));
  }

  location->DataSize = static_cast<uint32_t>(
      sizeof(simple_string_dictionary_entries) +
      entries.size() * sizeof(MinidumpSimpleStringDictionaryEntry));
}

// Writes |strings| to |writer| as a MinidumpRVAList referencing
// MinidumpUTF8String objects, and populates |location| with a location
// descriptor identifying what was written.
void WriteMinidumpStringList(MINIDUMP_LOCATION_DESCRIPTOR* location,
                             FileWriterInterface* writer,
                             const std::vector<std::string>& strings) {
  std::vector<RVA> rvas;
  for (const std::string& string : strings) {
    rvas.push_back(WriteString(writer, string));
  }

  location->Rva = static_cast<RVA>(writer->SeekGet());

  const uint32_t string_list_entries = static_cast<uint32_t>(rvas.size());
  EXPECT_TRUE(writer->Write(&string_list_entries, sizeof(string_list_entries)));
  for (RVA rva : rvas) {
    EXPECT_TRUE(writer->Write(&rva, sizeof(rva)));
  }

  location->DataSize = static_cast<uint32_t>(sizeof(string_list_entries) +
                                             rvas.size() * sizeof(RVA));
}

TEST(ProcessSnapshotMinidump, ClientID) {
  StringFile string_file;

  MINIDUMP_HEADER header = {};
  EXPECT_TRUE(string_file.Write(&header, sizeof(header)));

  UUID client_id;
  ASSERT_TRUE(
      client_id.InitializeFromString("0001f4a9-d00d-5155-0a55-c0ffeec0ffee"));

  MinidumpCrashpadInfo crashpad_info = {};
  crashpad_info.version = MinidumpCrashpadInfo::kVersion;
  crashpad_info.client_id = client_id;

  MINIDUMP_DIRECTORY crashpad_info_directory = {};
  crashpad_info_directory.StreamType = kMinidumpStreamTypeCrashpadInfo;
  crashpad_info_directory.Location.Rva =
      static_cast<RVA>(string_file.SeekGet());
  EXPECT_TRUE(string_file.Write(&crashpad_info, sizeof(crashpad_info)));
  crashpad_info_directory.Location.DataSize = sizeof(crashpad_info);

  header.StreamDirectoryRva = static_cast<RVA>(string_file.SeekGet());
  EXPECT_TRUE(string_file.Write(&crashpad_info_directory,
                                sizeof(crashpad_info_directory)));

  header.Signature = MINIDUMP_SIGNATURE;
  header.Version = MINIDUMP_VERSION;
  header.NumberOfStreams = 1;
  EXPECT_TRUE(string_file.SeekSet(0));
  EXPECT_TRUE(string_file.Write(&header, sizeof(header)));

  ProcessSnapshotMinidump process_snapshot;
  EXPECT_TRUE(process_snapshot.Initialize(&string_file));

  UUID actual_client_id;
  process_snapshot.ClientID(&actual_client_id);
  EXPECT_EQ(client_id, actual_client_id);

  EXPECT_TRUE(process_snapshot.AnnotationsSimpleMap().empty());
}

TEST(ProcessSnapshotMinidump, AnnotationsSimpleMap) {
  StringFile string_file;

  MINIDUMP_HEADER header = {};
  EXPECT_TRUE(string_file.Write(&header, sizeof(header)));

  MinidumpCrashpadInfo crashpad_info = {};
  crashpad_info.version = MinidumpCrashpadInfo::kVersion;

  std::map<std::string, std::string> dictionary;
  dictionary["the first key"] = "THE FIRST VALUE EVER!";
  dictionary["2key"] = "a lowly second value";
  WriteMinidumpSimpleStringDictionary(
      &crashpad_info.simple_annotations, &string_file, dictionary);

  MINIDUMP_DIRECTORY crashpad_info_directory = {};
  crashpad_info_directory.StreamType = kMinidumpStreamTypeCrashpadInfo;
  crashpad_info_directory.Location.Rva =
      static_cast<RVA>(string_file.SeekGet());
  EXPECT_TRUE(string_file.Write(&crashpad_info, sizeof(crashpad_info)));
  crashpad_info_directory.Location.DataSize = sizeof(crashpad_info);

  header.StreamDirectoryRva = static_cast<RVA>(string_file.SeekGet());
  EXPECT_TRUE(string_file.Write(&crashpad_info_directory,
                                sizeof(crashpad_info_directory)));

  header.Signature = MINIDUMP_SIGNATURE;
  header.Version = MINIDUMP_VERSION;
  header.NumberOfStreams = 1;
  EXPECT_TRUE(string_file.SeekSet(0));
  EXPECT_TRUE(string_file.Write(&header, sizeof(header)));

  ProcessSnapshotMinidump process_snapshot;
  EXPECT_TRUE(process_snapshot.Initialize(&string_file));

  UUID client_id;
  process_snapshot.ClientID(&client_id);
  EXPECT_EQ(UUID(), client_id);

  const auto annotations_simple_map = process_snapshot.AnnotationsSimpleMap();
  EXPECT_EQ(dictionary, annotations_simple_map);
}

TEST(ProcessSnapshotMinidump, Modules) {
  StringFile string_file;

  MINIDUMP_HEADER header = {};
  EXPECT_TRUE(string_file.Write(&header, sizeof(header)));

  MINIDUMP_MODULE minidump_module = {};
  uint32_t minidump_module_count = 3;

  MINIDUMP_DIRECTORY minidump_module_list_directory = {};
  minidump_module_list_directory.StreamType = kMinidumpStreamTypeModuleList;
  minidump_module_list_directory.Location.DataSize =
      sizeof(MINIDUMP_MODULE_LIST) +
      minidump_module_count * sizeof(MINIDUMP_MODULE);
  minidump_module_list_directory.Location.Rva =
      static_cast<RVA>(string_file.SeekGet());

  EXPECT_TRUE(
      string_file.Write(&minidump_module_count, sizeof(minidump_module_count)));
  for (uint32_t minidump_module_index = 0;
       minidump_module_index < minidump_module_count;
       ++minidump_module_index) {
    EXPECT_TRUE(string_file.Write(&minidump_module, sizeof(minidump_module)));
  }

  MinidumpModuleCrashpadInfo crashpad_module_0 = {};
  crashpad_module_0.version = MinidumpModuleCrashpadInfo::kVersion;
  std::map<std::string, std::string> dictionary_0;
  dictionary_0["ptype"] = "browser";
  dictionary_0["pid"] = "12345";
  WriteMinidumpSimpleStringDictionary(
      &crashpad_module_0.simple_annotations, &string_file, dictionary_0);

  MinidumpModuleCrashpadInfoLink crashpad_module_0_link = {};
  crashpad_module_0_link.minidump_module_list_index = 0;
  crashpad_module_0_link.location.DataSize = sizeof(crashpad_module_0);
  crashpad_module_0_link.location.Rva = static_cast<RVA>(string_file.SeekGet());
  EXPECT_TRUE(string_file.Write(&crashpad_module_0, sizeof(crashpad_module_0)));

  MinidumpModuleCrashpadInfo crashpad_module_2 = {};
  crashpad_module_2.version = MinidumpModuleCrashpadInfo::kVersion;
  std::map<std::string, std::string> dictionary_2;
  dictionary_2["fakemodule"] = "yes";
  WriteMinidumpSimpleStringDictionary(
      &crashpad_module_2.simple_annotations, &string_file, dictionary_2);

  std::vector<std::string> list_annotations_2;
  list_annotations_2.push_back("first string");
  list_annotations_2.push_back("last string");
  WriteMinidumpStringList(
      &crashpad_module_2.list_annotations, &string_file, list_annotations_2);

  MinidumpModuleCrashpadInfoLink crashpad_module_2_link = {};
  crashpad_module_2_link.minidump_module_list_index = 2;
  crashpad_module_2_link.location.DataSize = sizeof(crashpad_module_2);
  crashpad_module_2_link.location.Rva = static_cast<RVA>(string_file.SeekGet());
  EXPECT_TRUE(string_file.Write(&crashpad_module_2, sizeof(crashpad_module_2)));

  MinidumpCrashpadInfo crashpad_info = {};
  crashpad_info.version = MinidumpCrashpadInfo::kVersion;

  uint32_t crashpad_module_count = 2;

  crashpad_info.module_list.DataSize =
      sizeof(MinidumpModuleCrashpadInfoList) +
      crashpad_module_count * sizeof(MinidumpModuleCrashpadInfoLink);
  crashpad_info.module_list.Rva = static_cast<RVA>(string_file.SeekGet());

  EXPECT_TRUE(
      string_file.Write(&crashpad_module_count, sizeof(crashpad_module_count)));
  EXPECT_TRUE(string_file.Write(&crashpad_module_0_link,
                                sizeof(crashpad_module_0_link)));
  EXPECT_TRUE(string_file.Write(&crashpad_module_2_link,
                                sizeof(crashpad_module_2_link)));

  MINIDUMP_DIRECTORY crashpad_info_directory = {};
  crashpad_info_directory.StreamType = kMinidumpStreamTypeCrashpadInfo;
  crashpad_info_directory.Location.DataSize = sizeof(crashpad_info);
  crashpad_info_directory.Location.Rva =
      static_cast<RVA>(string_file.SeekGet());
  EXPECT_TRUE(string_file.Write(&crashpad_info, sizeof(crashpad_info)));

  header.StreamDirectoryRva = static_cast<RVA>(string_file.SeekGet());
  EXPECT_TRUE(string_file.Write(&minidump_module_list_directory,
                                sizeof(minidump_module_list_directory)));
  EXPECT_TRUE(string_file.Write(&crashpad_info_directory,
                                sizeof(crashpad_info_directory)));

  header.Signature = MINIDUMP_SIGNATURE;
  header.Version = MINIDUMP_VERSION;
  header.NumberOfStreams = 2;
  EXPECT_TRUE(string_file.SeekSet(0));
  EXPECT_TRUE(string_file.Write(&header, sizeof(header)));

  ProcessSnapshotMinidump process_snapshot;
  EXPECT_TRUE(process_snapshot.Initialize(&string_file));

  std::vector<const ModuleSnapshot*> modules = process_snapshot.Modules();
  ASSERT_EQ(minidump_module_count, modules.size());

  auto annotations_simple_map = modules[0]->AnnotationsSimpleMap();
  EXPECT_EQ(dictionary_0, annotations_simple_map);

  auto annotations_vector = modules[0]->AnnotationsVector();
  EXPECT_TRUE(annotations_vector.empty());

  annotations_simple_map = modules[1]->AnnotationsSimpleMap();
  EXPECT_TRUE(annotations_simple_map.empty());

  annotations_vector = modules[1]->AnnotationsVector();
  EXPECT_TRUE(annotations_vector.empty());

  annotations_simple_map = modules[2]->AnnotationsSimpleMap();
  EXPECT_EQ(dictionary_2, annotations_simple_map);

  annotations_vector = modules[2]->AnnotationsVector();
  EXPECT_EQ(list_annotations_2, annotations_vector);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
