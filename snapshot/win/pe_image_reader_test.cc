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

#include "snapshot/win/pe_image_reader.h"

#include <psapi.h>

#include "gtest/gtest.h"
#include "snapshot/win/process_reader_win.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace crashpad {
namespace test {
namespace {

TEST(PEImageReader, DebugDirectory) {
  PEImageReader pe_image_reader;
  ProcessReaderWin process_reader;
  ASSERT_TRUE(process_reader.Initialize(GetCurrentProcess()));
  HMODULE self = reinterpret_cast<HMODULE>(&__ImageBase);
  MODULEINFO module_info;
  ASSERT_TRUE(GetModuleInformation(
      GetCurrentProcess(), self, &module_info, sizeof(module_info)));
  EXPECT_EQ(self, module_info.lpBaseOfDll);
  EXPECT_TRUE(pe_image_reader.Initialize(&process_reader,
                                         reinterpret_cast<WinVMAddress>(self),
                                         module_info.SizeOfImage,
                                         "self"));
  UUID uuid;
  DWORD age;
  std::string pdbname;
  EXPECT_TRUE(pe_image_reader.DebugDirectoryInformation(&uuid, &age, &pdbname));
  EXPECT_NE(std::string::npos, pdbname.find("crashpad_snapshot_test"));
  const std::string suffix(".pdb");
  EXPECT_EQ(
      0,
      pdbname.compare(pdbname.size() - suffix.size(), suffix.size(), suffix));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
