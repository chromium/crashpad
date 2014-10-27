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
#include "minidump/minidump_module_crashpad_info_writer.h"
#include "minidump/test/minidump_file_writer_test_util.h"
#include "minidump/test/minidump_writable_test_util.h"
#include "util/file/string_file_writer.h"

namespace crashpad {
namespace test {
namespace {

void GetCrashpadInfoStream(const std::string& file_contents,
                           const MinidumpCrashpadInfo** crashpad_info,
                           const MinidumpModuleCrashpadInfoList** module_list) {
  const MINIDUMP_DIRECTORY* directory;
  const MINIDUMP_HEADER* header =
      MinidumpHeaderAtStart(file_contents, &directory);
  ASSERT_NO_FATAL_FAILURE(VerifyMinidumpHeader(header, 1, 0));
  ASSERT_TRUE(directory);

  ASSERT_EQ(kMinidumpStreamTypeCrashpadInfo, directory[0].StreamType);

  *crashpad_info = MinidumpWritableAtLocationDescriptor<MinidumpCrashpadInfo>(
      file_contents, directory[0].Location);
  ASSERT_TRUE(*crashpad_info);

  *module_list =
      MinidumpWritableAtLocationDescriptor<MinidumpModuleCrashpadInfoList>(
          file_contents, (*crashpad_info)->module_list);
}

TEST(MinidumpCrashpadInfoWriter, Empty) {
  MinidumpFileWriter minidump_file_writer;
  auto crashpad_info_writer = make_scoped_ptr(new MinidumpCrashpadInfoWriter());

  minidump_file_writer.AddStream(crashpad_info_writer.Pass());

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  const MinidumpCrashpadInfo* crashpad_info;
  const MinidumpModuleCrashpadInfoList* module_list;

  ASSERT_NO_FATAL_FAILURE(GetCrashpadInfoStream(
      file_writer.string(), &crashpad_info, &module_list));

  EXPECT_EQ(MinidumpCrashpadInfo::kVersion, crashpad_info->version);
  EXPECT_FALSE(module_list);
}

TEST(MinidumpCrashpadInfoWriter, CrashpadModuleList) {
  const uint32_t kMinidumpModuleListIndex = 3;

  MinidumpFileWriter minidump_file_writer;
  auto crashpad_info_writer = make_scoped_ptr(new MinidumpCrashpadInfoWriter());

  auto module_list_writer =
      make_scoped_ptr(new MinidumpModuleCrashpadInfoListWriter());
  auto module_writer = make_scoped_ptr(new MinidumpModuleCrashpadInfoWriter());
  module_writer->SetMinidumpModuleListIndex(kMinidumpModuleListIndex);
  module_list_writer->AddModule(module_writer.Pass());
  crashpad_info_writer->SetModuleList(module_list_writer.Pass());

  minidump_file_writer.AddStream(crashpad_info_writer.Pass());

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  const MinidumpCrashpadInfo* crashpad_info;
  const MinidumpModuleCrashpadInfoList* module_list;

  ASSERT_NO_FATAL_FAILURE(GetCrashpadInfoStream(
      file_writer.string(), &crashpad_info, &module_list));

  EXPECT_EQ(MinidumpCrashpadInfo::kVersion, crashpad_info->version);
  ASSERT_TRUE(module_list);
  ASSERT_EQ(1u, module_list->count);

  const MinidumpModuleCrashpadInfo* module =
      MinidumpWritableAtLocationDescriptor<MinidumpModuleCrashpadInfo>(
          file_writer.string(), module_list->modules[0]);
  ASSERT_TRUE(module);

  EXPECT_EQ(MinidumpModuleCrashpadInfo::kVersion, module->version);
  EXPECT_EQ(kMinidumpModuleListIndex, module->minidump_module_list_index);
  EXPECT_EQ(0u, module->simple_annotations.DataSize);
  EXPECT_EQ(0u, module->simple_annotations.Rva);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
