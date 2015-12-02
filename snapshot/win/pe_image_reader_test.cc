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

#define PSAPI_VERSION 1
#include <psapi.h>

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "gtest/gtest.h"
#include "snapshot/win/process_reader_win.h"
#include "test/errors.h"
#include "util/win/get_function.h"
#include "util/win/module_version.h"
#include "util/win/process_info.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace crashpad {
namespace test {
namespace {

BOOL CrashpadGetModuleInformation(HANDLE process,
                                  HMODULE module,
                                  MODULEINFO* module_info,
                                  DWORD cb) {
  static const auto get_module_information =
      GET_FUNCTION_REQUIRED(L"psapi.dll", ::GetModuleInformation);
  return get_module_information(process, module, module_info, cb);
}

TEST(PEImageReader, DebugDirectory) {
  PEImageReader pe_image_reader;
  ProcessReaderWin process_reader;
  ASSERT_TRUE(process_reader.Initialize(GetCurrentProcess(),
                                        ProcessSuspensionState::kRunning));
  HMODULE self = reinterpret_cast<HMODULE>(&__ImageBase);
  MODULEINFO module_info;
  ASSERT_TRUE(CrashpadGetModuleInformation(
      GetCurrentProcess(), self, &module_info, sizeof(module_info)))
      << ErrorMessage("GetModuleInformation");
  EXPECT_EQ(self, module_info.lpBaseOfDll);
  ASSERT_TRUE(pe_image_reader.Initialize(&process_reader,
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

void TestVSFixedFileInfo(ProcessReaderWin* process_reader,
                         const ProcessInfo::Module& module,
                         bool known_dll) {
  PEImageReader pe_image_reader;
  ASSERT_TRUE(pe_image_reader.Initialize(process_reader,
                                         module.dll_base,
                                         module.size,
                                         base::UTF16ToUTF8(module.name)));

  VS_FIXEDFILEINFO observed;
  const bool observed_rv = pe_image_reader.VSFixedFileInfo(&observed);
  ASSERT_TRUE(observed_rv || !known_dll);

  if (observed_rv) {
    EXPECT_EQ(VS_FFI_SIGNATURE, observed.dwSignature);
    EXPECT_EQ(VS_FFI_STRUCVERSION, observed.dwStrucVersion);
    EXPECT_EQ(0, observed.dwFileFlags & ~observed.dwFileFlagsMask);
    EXPECT_EQ(VOS_NT_WINDOWS32, observed.dwFileOS);
    if (known_dll) {
      EXPECT_EQ(VFT_DLL, observed.dwFileType);
    } else {
      EXPECT_TRUE(observed.dwFileType == VFT_APP ||
                  observed.dwFileType == VFT_DLL);
    }
  }

  base::FilePath module_path(module.name);

  const DWORD version = GetVersion();
  const int major_version = LOBYTE(LOWORD(version));
  const int minor_version = HIBYTE(LOWORD(version));
  if (major_version > 6 || (major_version == 6 && minor_version >= 2)) {
    // Windows 8 or later.
    //
    // Use BaseName() to ensure that GetModuleVersionAndType() finds the
    // already-loaded module with the specified name. Otherwise, dwFileVersionMS
    // may not match. This appears to be related to the changes made in Windows
    // 8.1 to GetVersion() and GetVersionEx() for non-manifested applications
    module_path = module_path.BaseName();
  }

  VS_FIXEDFILEINFO expected;
  const bool expected_rv = GetModuleVersionAndType(module_path, &expected);
  ASSERT_TRUE(expected_rv || !known_dll);

  EXPECT_EQ(expected_rv, observed_rv);

  if (observed_rv && expected_rv) {
    EXPECT_EQ(expected.dwSignature, observed.dwSignature);
    EXPECT_EQ(expected.dwStrucVersion, observed.dwStrucVersion);
    EXPECT_EQ(expected.dwFileVersionMS, observed.dwFileVersionMS);
    EXPECT_EQ(expected.dwFileVersionLS, observed.dwFileVersionLS);
    EXPECT_EQ(expected.dwProductVersionMS, observed.dwProductVersionMS);
    EXPECT_EQ(expected.dwProductVersionLS, observed.dwProductVersionLS);
    EXPECT_EQ(expected.dwFileFlagsMask, observed.dwFileFlagsMask);
    EXPECT_EQ(expected.dwFileFlags, observed.dwFileFlags);
    EXPECT_EQ(expected.dwFileOS, observed.dwFileOS);
    EXPECT_EQ(expected.dwFileType, observed.dwFileType);
    EXPECT_EQ(expected.dwFileSubtype, observed.dwFileSubtype);
    EXPECT_EQ(expected.dwFileDateMS, observed.dwFileDateMS);
    EXPECT_EQ(expected.dwFileDateLS, observed.dwFileDateLS);
  }
}

TEST(PEImageReader, VSFixedFileInfo_OneModule) {
  ProcessReaderWin process_reader;
  ASSERT_TRUE(process_reader.Initialize(GetCurrentProcess(),
                                        ProcessSuspensionState::kRunning));

  const wchar_t kModuleName[] = L"kernel32.dll";
  const HMODULE module_handle = GetModuleHandle(kModuleName);
  ASSERT_TRUE(module_handle) << ErrorMessage("GetModuleHandle");

  MODULEINFO module_info;
  ASSERT_TRUE(CrashpadGetModuleInformation(
      GetCurrentProcess(), module_handle, &module_info, sizeof(module_info)))
      << ErrorMessage("GetModuleInformation");
  EXPECT_EQ(module_handle, module_info.lpBaseOfDll);

  ProcessInfo::Module module;
  module.name = kModuleName;
  module.dll_base = reinterpret_cast<WinVMAddress>(module_info.lpBaseOfDll);
  module.size = module_info.SizeOfImage;

  TestVSFixedFileInfo(&process_reader, module, true);
}

TEST(PEImageReader, VSFixedFileInfo_AllModules) {
  ProcessReaderWin process_reader;
  ASSERT_TRUE(process_reader.Initialize(GetCurrentProcess(),
                                        ProcessSuspensionState::kRunning));

  const std::vector<ProcessInfo::Module>& modules = process_reader.Modules();
  EXPECT_GT(modules.size(), 2u);

  for (const auto& module : modules) {
    SCOPED_TRACE(base::UTF16ToUTF8(module.name));
    TestVSFixedFileInfo(&process_reader, module, false);
  }
}

}  // namespace
}  // namespace test
}  // namespace crashpad
