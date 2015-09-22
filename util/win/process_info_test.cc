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

#include "util/win/process_info.h"

#include <imagehlp.h>
#include <wchar.h>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "test/paths.h"
#include "test/win/child_launcher.h"
#include "util/file/file_io.h"
#include "util/misc/uuid.h"
#include "util/win/scoped_handle.h"

namespace crashpad {
namespace test {
namespace {

const wchar_t kNtdllName[] = L"\\ntdll.dll";

time_t GetTimestampForModule(HMODULE module) {
  char filename[MAX_PATH];
  // `char` and GetModuleFileNameA because ImageLoad is ANSI only.
  if (!GetModuleFileNameA(module, filename, arraysize(filename)))
    return 0;
  LOADED_IMAGE* loaded_image = ImageLoad(filename, nullptr);
  return loaded_image->FileHeader->FileHeader.TimeDateStamp;
}

bool IsProcessWow64(HANDLE process_handle) {
  static decltype(IsWow64Process)* is_wow64_process =
      reinterpret_cast<decltype(IsWow64Process)*>(
          GetProcAddress(LoadLibrary(L"kernel32.dll"), "IsWow64Process"));
  if (!is_wow64_process)
    return false;
  BOOL is_wow64;
  if (!is_wow64_process(process_handle, &is_wow64)) {
    PLOG(ERROR) << "IsWow64Process";
    return false;
  }
  return is_wow64;
}

TEST(ProcessInfo, Self) {
  ProcessInfo process_info;
  ASSERT_TRUE(process_info.Initialize(GetCurrentProcess()));
  EXPECT_EQ(GetCurrentProcessId(), process_info.ProcessID());
  EXPECT_GT(process_info.ParentProcessID(), 0u);

#if defined(ARCH_CPU_64_BITS)
  EXPECT_TRUE(process_info.Is64Bit());
  EXPECT_FALSE(process_info.IsWow64());
#else
  EXPECT_FALSE(process_info.Is64Bit());
  if (IsProcessWow64(GetCurrentProcess()))
    EXPECT_TRUE(process_info.IsWow64());
  else
    EXPECT_FALSE(process_info.IsWow64());
#endif

  std::wstring command_line;
  EXPECT_TRUE(process_info.CommandLine(&command_line));
  EXPECT_EQ(std::wstring(GetCommandLine()), command_line);

  std::vector<ProcessInfo::Module> modules;
  EXPECT_TRUE(process_info.Modules(&modules));
  ASSERT_GE(modules.size(), 2u);
  const wchar_t kSelfName[] = L"\\crashpad_util_test.exe";
  ASSERT_GE(modules[0].name.size(), wcslen(kSelfName));
  EXPECT_EQ(kSelfName,
            modules[0].name.substr(modules[0].name.size() - wcslen(kSelfName)));
  ASSERT_GE(modules[1].name.size(), wcslen(kNtdllName));
  EXPECT_EQ(
      kNtdllName,
      modules[1].name.substr(modules[1].name.size() - wcslen(kNtdllName)));

  EXPECT_EQ(modules[0].dll_base,
            reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr)));
  EXPECT_EQ(modules[1].dll_base,
            reinterpret_cast<uintptr_t>(GetModuleHandle(L"ntdll.dll")));

  EXPECT_GT(modules[0].size, 0);
  EXPECT_GT(modules[1].size, 0);

  EXPECT_EQ(modules[0].timestamp,
            GetTimestampForModule(GetModuleHandle(nullptr)));
  // System modules are forced to particular stamps and the file header values
  // don't match the on-disk times. Just make sure we got some data here.
  EXPECT_GT(modules[1].timestamp, 0);
}

void TestOtherProcess(const base::string16& directory_modification) {
  ProcessInfo process_info;

  UUID started_uuid(UUID::InitializeWithNewTag{});
  UUID done_uuid(UUID::InitializeWithNewTag{});

  ScopedKernelHANDLE started(
      CreateEvent(nullptr, true, false, started_uuid.ToString16().c_str()));
  ASSERT_TRUE(started.get());
  ScopedKernelHANDLE done(
      CreateEvent(nullptr, true, false, done_uuid.ToString16().c_str()));
  ASSERT_TRUE(done.get());

  base::FilePath test_executable = Paths::Executable();

  std::wstring child_test_executable =
      test_executable.DirName()
          .Append(directory_modification)
          .Append(test_executable.BaseName().RemoveFinalExtension().value() +
                  L"_process_info_test_child.exe")
          .value();

  std::wstring args;
  AppendCommandLineArgument(started_uuid.ToString16(), &args);
  args += L" ";
  AppendCommandLineArgument(done_uuid.ToString16(), &args);

  ChildLauncher child(child_test_executable, args);
  child.Start();

  // Wait until the test has completed initialization.
  ASSERT_EQ(WaitForSingleObject(started.get(), INFINITE), WAIT_OBJECT_0);

  ASSERT_TRUE(process_info.Initialize(child.process_handle()));

  // Tell the test it's OK to shut down now that we've read our data.
  SetEvent(done.get());

  std::vector<ProcessInfo::Module> modules;
  EXPECT_TRUE(process_info.Modules(&modules));
  ASSERT_GE(modules.size(), 3u);
  std::wstring child_name = L"\\crashpad_util_test_process_info_test_child.exe";
  ASSERT_GE(modules[0].name.size(), child_name.size());
  EXPECT_EQ(child_name,
            modules[0].name.substr(modules[0].name.size() - child_name.size()));
  ASSERT_GE(modules[1].name.size(), wcslen(kNtdllName));
  EXPECT_EQ(
      kNtdllName,
      modules[1].name.substr(modules[1].name.size() - wcslen(kNtdllName)));
  // lz32.dll is an uncommonly-used-but-always-available module that the test
  // binary manually loads.
  const wchar_t kLz32dllName[] = L"\\lz32.dll";
  ASSERT_GE(modules.back().name.size(), wcslen(kLz32dllName));
  EXPECT_EQ(kLz32dllName,
            modules.back().name.substr(modules.back().name.size() -
                                       wcslen(kLz32dllName)));
}

TEST(ProcessInfo, OtherProcess) {
  TestOtherProcess(FILE_PATH_LITERAL("."));
}

#if defined(ARCH_CPU_64_BITS)
TEST(ProcessInfo, OtherProcessWOW64) {
#ifndef NDEBUG
  TestOtherProcess(FILE_PATH_LITERAL("..\\..\\out\\Debug"));
#else
  TestOtherProcess(FILE_PATH_LITERAL("..\\..\\out\\Release"));
#endif
}
#endif  // ARCH_CPU_64_BITS

}  // namespace
}  // namespace test
}  // namespace crashpad
