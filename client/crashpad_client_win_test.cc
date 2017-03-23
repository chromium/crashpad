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

#include "client/crashpad_client.h"

#include <tlhelp32.h>

#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "gtest/gtest.h"
#include "test/paths.h"
#include "test/scoped_temp_dir.h"
#include "test/win/win_multiprocess.h"
#include "util/win/scoped_handle.h"
#include "util/win/termination_codes.h"

namespace crashpad {
namespace test {
namespace {

// Returns all processes that have the process ID associated with parent as
// parent process ID. This is a strict superset of its sub-processes.
bool GetPotentialChildProcessesOf(HANDLE parent,
                                  std::vector<DWORD>* processes) {
  ScopedKernelHANDLE snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
  if (!snapshot.is_valid())
    return false;

  PROCESSENTRY32 entry = {sizeof(entry)};
  DWORD pid = GetProcessId(parent);
  if (!Process32First(snapshot.get(), &entry))
    return false;

  do {
    if (entry.th32ParentProcessID == pid)
      processes->push_back(entry.th32ProcessID);
  } while (Process32Next(snapshot.get(), &entry));

  return true;
}

ULARGE_INTEGER GetProcessCreationTime(HANDLE process) {
  ULARGE_INTEGER ret = {};
  FILETIME creation_time;
  FILETIME dummy;
  if (GetProcessTimes(process, &creation_time, &dummy, &dummy, &dummy)) {
    ret.LowPart = creation_time.dwLowDateTime;
    ret.HighPart = creation_time.dwHighDateTime;
  }

  return ret;
}

// Waits for the processes directly created by |parent| - and specifically not
// their offspring. For this to work without race, |parent| has to be suspended
// or have exited.
bool WaitForAllChildProcessesOf(HANDLE parent) {
  std::vector<DWORD> subprocess_pids;
  if (!GetPotentialChildProcessesOf(parent, &subprocess_pids))
    return false;

  ULARGE_INTEGER parent_creationtime = GetProcessCreationTime(parent);
  for (DWORD pid : subprocess_pids) {
    // Try and open the process. This may fail for reasons such as:
    // 1. The process isn't our subprocess, but rather a higher-privilege
    //    sub-process of an earlier process that had our pid.
    // 2. The process no longer exists, e.g. it exited after enumeration.
    ScopedKernelHANDLE process(
        OpenProcess(PROCESS_QUERY_INFORMATION | SYNCHRONIZE, false, pid));
    if (!process.is_valid())
      continue;

    // We successfully opened the process, but this could still be a sub-process
    // of another process that earlier had our pid. To make sure, check that the
    // process we opened was created after our own process.
    ULARGE_INTEGER process_creationtime = GetProcessCreationTime(process.get());
    if (process_creationtime.QuadPart <= parent_creationtime.QuadPart)
      continue;

    if (WaitForSingleObject(process.get(), INFINITE) != WAIT_OBJECT_0)
      return false;
  }

  return true;
}

void StartAndUseHandler(const base::FilePath& temp_dir) {
  base::FilePath handler_path = Paths::Executable().DirName().Append(
      FILE_PATH_LITERAL("crashpad_handler.com"));

  {
    CrashpadClient client;
    ASSERT_TRUE(client.StartHandler(handler_path,
                                    temp_dir,
                                    base::FilePath(),
                                    "",
                                    std::map<std::string, std::string>(),
                                    std::vector<std::string>(),
                                    true,
                                    true));
    ASSERT_TRUE(client.WaitForHandlerStart(INFINITE));
  }
}

constexpr wchar_t kTempDirEnvName[] = L"CRASHPAD_TEST_TEMP_DIR";

class StartWithInvalidHandles final : public WinMultiprocess {
 public:
  StartWithInvalidHandles() : WinMultiprocess() {}
  ~StartWithInvalidHandles() {}

 private:
  void WinMultiprocessParentBeforeChild() override {
    temp_dir_ = base::WrapUnique(new ScopedTempDir);

    ::SetEnvironmentVariable(kTempDirEnvName,
                             temp_dir_->path().value().c_str());
  }
  void WinMultiprocessParent() override {}
  void WinMultiprocessParentAfterChild(HANDLE child) override {
    WaitForAllChildProcessesOf(child);
    temp_dir_.reset();
  }

  void WinMultiprocessChild() override {
    HANDLE original_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE original_stderr = GetStdHandle(STD_ERROR_HANDLE);
    SetStdHandle(STD_OUTPUT_HANDLE, INVALID_HANDLE_VALUE);
    SetStdHandle(STD_ERROR_HANDLE, INVALID_HANDLE_VALUE);

    wchar_t temp_dir_path[MAX_PATH] = {};
    ::GetEnvironmentVariable(
        kTempDirEnvName, temp_dir_path, arraysize(temp_dir_path));

    StartAndUseHandler(base::FilePath(temp_dir_path));

    SetStdHandle(STD_OUTPUT_HANDLE, original_stdout);
    SetStdHandle(STD_ERROR_HANDLE, original_stderr);
  }

  std::unique_ptr<ScopedTempDir> temp_dir_;
};

TEST(CrashpadClient, StartWithInvalidHandles) {
  WinMultiprocess::Run<StartWithInvalidHandles>();
}

class StartWithSameStdoutStderr final : public WinMultiprocess {
 public:
  StartWithSameStdoutStderr() : WinMultiprocess() {}
  ~StartWithSameStdoutStderr() {}

 private:
  void WinMultiprocessParent() override {}

  void WinMultiprocessChild() override {
    HANDLE original_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE original_stderr = GetStdHandle(STD_ERROR_HANDLE);
    SetStdHandle(STD_OUTPUT_HANDLE, original_stderr);

    ScopedTempDir temp_dir;
    StartAndUseHandler(temp_dir.path());

    SetStdHandle(STD_OUTPUT_HANDLE, original_stdout);
  }
};

TEST(CrashpadClient, StartWithSameStdoutStderr) {
  WinMultiprocess::Run<StartWithSameStdoutStderr>();
}

void StartAndUseBrokenHandler(CrashpadClient* client) {
  ScopedTempDir temp_dir;
  base::FilePath handler_path = Paths::Executable().DirName().Append(
      FILE_PATH_LITERAL("fake_handler_that_crashes_at_startup.exe"));
  ASSERT_TRUE(client->StartHandler(handler_path,
                                  temp_dir.path(),
                                  base::FilePath(),
                                  "",
                                  std::map<std::string, std::string>(),
                                  std::vector<std::string>(),
                                  false,
                                  true));
}

class HandlerLaunchFailureCrash : public WinMultiprocess {
 public:
  HandlerLaunchFailureCrash() : WinMultiprocess() {}

 private:
  void WinMultiprocessParent() override {
    SetExpectedChildExitCode(crashpad::kTerminationCodeCrashNoDump);
  }

  void WinMultiprocessChild() override {
    CrashpadClient client;
    StartAndUseBrokenHandler(&client);
    __debugbreak();
    exit(0);
  }
};

TEST(CrashpadClient, HandlerLaunchFailureCrash) {
  WinMultiprocess::Run<HandlerLaunchFailureCrash>();
}

class HandlerLaunchFailureDumpAndCrash : public WinMultiprocess {
 public:
  HandlerLaunchFailureDumpAndCrash() : WinMultiprocess() {}

 private:
  void WinMultiprocessParent() override {
    SetExpectedChildExitCode(crashpad::kTerminationCodeCrashNoDump);
  }

  void WinMultiprocessChild() override {
    CrashpadClient client;
    StartAndUseBrokenHandler(&client);
    // We don't need to fill this out as we're only checking that we're
    // terminated with the correct failure code.
    EXCEPTION_POINTERS info = {};
    client.DumpAndCrash(&info);
    exit(0);
  }
};

TEST(CrashpadClient, HandlerLaunchFailureDumpAndCrash) {
  WinMultiprocess::Run<HandlerLaunchFailureDumpAndCrash>();
}

class HandlerLaunchFailureDumpWithoutCrash : public WinMultiprocess {
 public:
  HandlerLaunchFailureDumpWithoutCrash() : WinMultiprocess() {}

 private:
  void WinMultiprocessParent() override {
    // DumpWithoutCrash() normally blocks indefinitely. There's no return value,
    // but confirm that it exits cleanly because it'll return right away if the
    // handler didn't start.
    SetExpectedChildExitCode(0);
  }

  void WinMultiprocessChild() override {
    CrashpadClient client;
    StartAndUseBrokenHandler(&client);
    // We don't need to fill this out as we're only checking that we're
    // terminated with the correct failure code.
    CONTEXT context = {};
    client.DumpWithoutCrash(context);
    exit(0);
  }
};

TEST(CrashpadClient, HandlerLaunchFailureDumpWithoutCrash) {
  WinMultiprocess::Run<HandlerLaunchFailureDumpWithoutCrash>();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
