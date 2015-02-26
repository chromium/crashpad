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

#include <string.h>
#include <windows.h>
#include <dbghelp.h>

#include "base/memory/scoped_ptr.h"
#include "gtest/gtest.h"
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
}

TEST(ProcessSnapshotMinidump, AnnotationsSimpleMap) {
  StringFile string_file;

  MINIDUMP_HEADER header = {};
  EXPECT_TRUE(string_file.Write(&header, sizeof(header)));

  MinidumpSimpleStringDictionaryEntry entry0 = {};
  entry0.key = static_cast<RVA>(string_file.SeekGet());
  const char kKey0[] = "the first key";
  uint32_t string_size = strlen(kKey0);
  EXPECT_TRUE(string_file.Write(&string_size, sizeof(string_size)));
  EXPECT_TRUE(string_file.Write(kKey0, sizeof(kKey0)));

  entry0.value = static_cast<RVA>(string_file.SeekGet());
  const char kValue0[] = "THE FIRST VALUE EVER!";
  string_size = strlen(kValue0);
  EXPECT_TRUE(string_file.Write(&string_size, sizeof(string_size)));
  EXPECT_TRUE(string_file.Write(kValue0, sizeof(kValue0)));

  MinidumpSimpleStringDictionaryEntry entry1 = {};
  entry1.key = static_cast<RVA>(string_file.SeekGet());
  const char kKey1[] = "2key";
  string_size = strlen(kKey1);
  EXPECT_TRUE(string_file.Write(&string_size, sizeof(string_size)));
  EXPECT_TRUE(string_file.Write(kKey1, sizeof(kKey1)));

  entry1.value = static_cast<RVA>(string_file.SeekGet());
  const char kValue1[] = "a lowly second value";
  string_size = strlen(kValue1);
  EXPECT_TRUE(string_file.Write(&string_size, sizeof(string_size)));
  EXPECT_TRUE(string_file.Write(kValue1, sizeof(kValue1)));

  MinidumpCrashpadInfo crashpad_info = {};
  crashpad_info.version = MinidumpCrashpadInfo::kVersion;

  crashpad_info.simple_annotations.Rva =
      static_cast<RVA>(string_file.SeekGet());
  uint32_t simple_string_dictionary_entries = 2;
  EXPECT_TRUE(string_file.Write(&simple_string_dictionary_entries,
                                sizeof(simple_string_dictionary_entries)));
  EXPECT_TRUE(string_file.Write(&entry0, sizeof(entry0)));
  EXPECT_TRUE(string_file.Write(&entry1, sizeof(entry1)));
  crashpad_info.simple_annotations.DataSize =
      simple_string_dictionary_entries *
      sizeof(MinidumpSimpleStringDictionaryEntry);

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

  const auto annotations_simple_map = process_snapshot.AnnotationsSimpleMap();
  EXPECT_EQ(2u, annotations_simple_map.size());

  auto it = annotations_simple_map.find(kKey0);
  ASSERT_NE(it, annotations_simple_map.end());
  EXPECT_EQ(kValue0, it->second);

  it = annotations_simple_map.find(kKey1);
  ASSERT_NE(it, annotations_simple_map.end());
  EXPECT_EQ(kValue1, it->second);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
