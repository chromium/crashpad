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

#include <rpc.h>
#include <wchar.h>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "test/paths.h"
#include "util/file/file_io.h"
#include "util/misc/uuid.h"
#include "util/win/scoped_handle.h"

namespace crashpad {
namespace test {
namespace {

const wchar_t kNtdllName[] = L"\\ntdll.dll";

time_t GetTimestampForModule(HMODULE module) {
  wchar_t filename[MAX_PATH];
  if (!GetModuleFileName(module, filename, arraysize(filename)))
    return 0;
  struct _stat stat_buf;
  int rv = _wstat(filename, &stat_buf);
  EXPECT_EQ(0, rv);
  if (rv != 0)
    return 0;
  return stat_buf.st_mtime;
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
  // Assume we won't be running these tests on a 32 bit host OS.
  EXPECT_TRUE(process_info.IsWow64());
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

void TestOtherProcess(const std::wstring& child_name_suffix) {
  ProcessInfo process_info;

  ::UUID system_uuid;
  ASSERT_EQ(RPC_S_OK, UuidCreate(&system_uuid));
  UUID started_uuid(reinterpret_cast<const uint8_t*>(&system_uuid.Data1));
  ASSERT_EQ(RPC_S_OK, UuidCreate(&system_uuid));
  UUID done_uuid(reinterpret_cast<const uint8_t*>(&system_uuid.Data1));

  ScopedKernelHANDLE started(
      CreateEvent(nullptr, true, false, started_uuid.ToString16().c_str()));
  ASSERT_TRUE(started.get());
  ScopedKernelHANDLE done(
      CreateEvent(nullptr, true, false, done_uuid.ToString16().c_str()));
  ASSERT_TRUE(done.get());

  base::FilePath test_executable = Paths::Executable();
  std::wstring child_test_executable =
      test_executable.RemoveFinalExtension().value() +
      L"_process_info_test_child_" + child_name_suffix + L".exe";
  // TODO(scottmg): Command line escaping utility.
  std::wstring command_line = child_test_executable + L" " +
                              started_uuid.ToString16() + L" " +
                              done_uuid.ToString16();
  STARTUPINFO startup_info = {0};
  startup_info.cb = sizeof(startup_info);
  PROCESS_INFORMATION process_information;
  ASSERT_TRUE(CreateProcess(child_test_executable.c_str(),
                            &command_line[0],
                            nullptr,
                            nullptr,
                            false,
                            0,
                            nullptr,
                            nullptr,
                            &startup_info,
                            &process_information));
  // Take ownership of the two process handles returned.
  ScopedKernelHANDLE process_main_thread_handle(process_information.hThread);
  ScopedKernelHANDLE process_handle(process_information.hProcess);

  // Wait until the test has completed initialization.
  ASSERT_EQ(WaitForSingleObject(started.get(), INFINITE), WAIT_OBJECT_0);

  ASSERT_TRUE(process_info.Initialize(process_information.hProcess));

  // Tell the test it's OK to shut down now that we've read our data.
  SetEvent(done.get());

  std::vector<ProcessInfo::Module> modules;
  EXPECT_TRUE(process_info.Modules(&modules));
  ASSERT_GE(modules.size(), 3u);
  std::wstring child_name = L"\\crashpad_util_test_process_info_test_child_" +
                            child_name_suffix + L".exe";
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

TEST(ProcessInfo, OtherProcessX64) {
  TestOtherProcess(L"x64");
}

TEST(ProcessInfo, OtherProcessX86) {
  TestOtherProcess(L"x86");
}

}  // namespace
}  // namespace test
}  // namespace crashpad
