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

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "gtest/gtest.h"
#include "minidump/minidump_file_writer.h"
#include "minidump/minidump_memory_info_writer.h"
#include "minidump/test/minidump_file_writer_test_util.h"
#include "minidump/test/minidump_writable_test_util.h"
#include "snapshot/test/test_memory_map_region_snapshot.h"
#include "util/file/string_file.h"

namespace crashpad {
namespace test {
namespace {

// The memory info list is expected to be the only stream.
void GetMemoryInfoListStream(
    const std::string& file_contents,
    const MINIDUMP_MEMORY_INFO_LIST** memory_info_list) {
  const size_t kDirectoryOffset = sizeof(MINIDUMP_HEADER);
  const size_t kMemoryInfoListStreamOffset =
      kDirectoryOffset + sizeof(MINIDUMP_DIRECTORY);

  const MINIDUMP_DIRECTORY* directory;
  const MINIDUMP_HEADER* header =
      MinidumpHeaderAtStart(file_contents, &directory);
  ASSERT_NO_FATAL_FAILURE(VerifyMinidumpHeader(header, 1, 0));
  ASSERT_TRUE(directory);

  const size_t kDirectoryIndex = 0;

  ASSERT_EQ(kMinidumpStreamTypeMemoryInfoList,
            directory[kDirectoryIndex].StreamType);
  EXPECT_EQ(kMemoryInfoListStreamOffset,
            directory[kDirectoryIndex].Location.Rva);

  *memory_info_list =
      MinidumpWritableAtLocationDescriptor<MINIDUMP_MEMORY_INFO_LIST>(
          file_contents, directory[kDirectoryIndex].Location);
  ASSERT_TRUE(*memory_info_list);
}

TEST(MinidumpMemoryInfoWriter, Empty) {
  MinidumpFileWriter minidump_file_writer;
  auto memory_info_list_writer =
      base::WrapUnique(new MinidumpMemoryInfoListWriter());
  minidump_file_writer.AddStream(std::move(memory_info_list_writer));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  ASSERT_EQ(sizeof(MINIDUMP_HEADER) + sizeof(MINIDUMP_DIRECTORY) +
                sizeof(MINIDUMP_MEMORY_INFO_LIST),
            string_file.string().size());

  const MINIDUMP_MEMORY_INFO_LIST* memory_info_list = nullptr;
  ASSERT_NO_FATAL_FAILURE(
      GetMemoryInfoListStream(string_file.string(), &memory_info_list));

  EXPECT_EQ(0u, memory_info_list->NumberOfEntries);
}

TEST(MinidumpMemoryInfoWriter, OneRegion) {
  MinidumpFileWriter minidump_file_writer;
  auto memory_info_list_writer =
      base::WrapUnique(new MinidumpMemoryInfoListWriter());

  auto memory_map_region = base::WrapUnique(new TestMemoryMapRegionSnapshot());

  MINIDUMP_MEMORY_INFO mmi = {0};
  mmi.BaseAddress = 0x12340000;
  mmi.AllocationBase = 0x12000000;
  mmi.AllocationProtect = PAGE_READWRITE;
  mmi.RegionSize = 0x6000;
  mmi.State = MEM_COMMIT;
  mmi.Protect = PAGE_NOACCESS;
  mmi.Type = MEM_PRIVATE;
  memory_map_region->SetMindumpMemoryInfo(mmi);

  std::vector<const MemoryMapRegionSnapshot*> memory_map;
  memory_map.push_back(memory_map_region.get());
  memory_info_list_writer->InitializeFromSnapshot(memory_map);

  minidump_file_writer.AddStream(std::move(memory_info_list_writer));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  ASSERT_EQ(sizeof(MINIDUMP_HEADER) + sizeof(MINIDUMP_DIRECTORY) +
                sizeof(MINIDUMP_MEMORY_INFO_LIST) +
                sizeof(MINIDUMP_MEMORY_INFO),
            string_file.string().size());

  const MINIDUMP_MEMORY_INFO_LIST* memory_info_list = nullptr;
  ASSERT_NO_FATAL_FAILURE(
      GetMemoryInfoListStream(string_file.string(), &memory_info_list));

  EXPECT_EQ(1u, memory_info_list->NumberOfEntries);
  const MINIDUMP_MEMORY_INFO* memory_info =
      reinterpret_cast<const MINIDUMP_MEMORY_INFO*>(&memory_info_list[1]);
  EXPECT_EQ(mmi.BaseAddress, memory_info->BaseAddress);
  EXPECT_EQ(mmi.AllocationBase, memory_info->AllocationBase);
  EXPECT_EQ(mmi.AllocationProtect, memory_info->AllocationProtect);
  EXPECT_EQ(mmi.RegionSize, memory_info->RegionSize);
  EXPECT_EQ(mmi.State, memory_info->State);
  EXPECT_EQ(mmi.Protect, memory_info->Protect);
  EXPECT_EQ(mmi.Type, memory_info->Type);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
