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
#include "util/misc/uuid.h"
#include "util/test/executable_path.h"
#include "util/win/scoped_handle.h"

namespace crashpad {
namespace test {
namespace {

const wchar_t kNtdllName[] = L"\\ntdll.dll";

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

  std::vector<std::wstring> modules;
  EXPECT_TRUE(process_info.Modules(&modules));
  ASSERT_GE(modules.size(), 2u);
  const wchar_t kSelfName[] = L"\\util_test.exe";
  ASSERT_GE(modules[0].size(), wcslen(kSelfName));
  EXPECT_EQ(kSelfName,
            modules[0].substr(modules[0].size() - wcslen(kSelfName)));
  ASSERT_GE(modules[1].size(), wcslen(kNtdllName));
  EXPECT_EQ(kNtdllName,
            modules[1].substr(modules[1].size() - wcslen(kNtdllName)));
}

TEST(ProcessInfo, SomeOtherProcess) {
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

  base::FilePath test_executable = ExecutablePath();
  std::wstring child_test_executable =
      test_executable.RemoveFinalExtension().value() +
      L"_process_info_test_child.exe";
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

  std::vector<std::wstring> modules;
  EXPECT_TRUE(process_info.Modules(&modules));
  ASSERT_GE(modules.size(), 3u);
  const wchar_t kChildName[] = L"\\util_test_process_info_test_child.exe";
  ASSERT_GE(modules[0].size(), wcslen(kChildName));
  EXPECT_EQ(kChildName,
            modules[0].substr(modules[0].size() - wcslen(kChildName)));
  ASSERT_GE(modules[1].size(), wcslen(kNtdllName));
  EXPECT_EQ(kNtdllName,
            modules[1].substr(modules[1].size() - wcslen(kNtdllName)));
  // lz32.dll is an uncommonly-used-but-always-available module that the test
  // binary manually loads.
  const wchar_t kLz32dllName[] = L"\\lz32.dll";
  ASSERT_GE(modules.back().size(), wcslen(kLz32dllName));
  EXPECT_EQ(
      kLz32dllName,
      modules.back().substr(modules.back().size() - wcslen(kLz32dllName)));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
