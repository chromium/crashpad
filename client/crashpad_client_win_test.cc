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
#include "base/logging.h"
#include "gtest/gtest.h"
#include "test/errors.h"
#include "test/paths.h"
#include "test/scoped_temp_dir.h"
#include "util/win/process_info.h"
#include "test/win/win_multiprocess.h"
#include "util/win/scoped_handle.h"
#include "util/win/termination_codes.h"

namespace crashpad {
namespace test {
namespace {

class ScopedEnvironmentVariable {
 public:
  explicit ScopedEnvironmentVariable(const wchar_t* name);
  ~ScopedEnvironmentVariable();

  std::wstring GetValue() const;

  // Sets this environment variable to |new_value|. If |new_value| is nullptr
  // this environment variable will be undefined.
  void SetValue(const wchar_t* new_value) const;

 private:
  std::wstring GetValueImpl(bool* is_defined) const;

  std::wstring original_value_;
  const wchar_t* name_;
  bool was_defined_;

  DISALLOW_COPY_AND_ASSIGN(ScopedEnvironmentVariable);
};

ScopedEnvironmentVariable::ScopedEnvironmentVariable(const wchar_t* name)
    : name_(name) {
  original_value_ = GetValueImpl(&was_defined_);
}

ScopedEnvironmentVariable::~ScopedEnvironmentVariable() {
  if (was_defined_)
    SetValue(original_value_.data());
  else
    SetValue(nullptr);
}

std::wstring ScopedEnvironmentVariable::GetValue() const {
  bool dummy;
  return GetValueImpl(&dummy);
}

std::wstring ScopedEnvironmentVariable::GetValueImpl(bool* is_defined) const {
  // The length returned is inclusive of the terminating zero, except
  // if the variable doesn't exist, in which case the return value is zero.
  DWORD len = GetEnvironmentVariable(name_, nullptr, 0);
  if (len == 0) {
    *is_defined = false;
    return L"";
  }

  *is_defined = true;

  std::wstring ret;
  ret.resize(len);
  // The length returned on success is exclusive of the terminating zero.
  len = GetEnvironmentVariable(name_, &ret[0], len);
  ret.resize(len);

  return ret;
}

void ScopedEnvironmentVariable::SetValue(const wchar_t* new_value) const {
  SetEnvironmentVariable(name_, new_value);
}

// Returns the process IDs of all processes that have |parent_pid| as
// parent process ID.
std::vector<pid_t> GetPotentialChildProcessesOf(pid_t parent_pid) {
  ScopedFileHANDLE snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
  if (!snapshot.is_valid()) {
    ADD_FAILURE() << ErrorMessage("CreateToolhelp32Snapshot");
    return std::vector<pid_t>();
  }

  PROCESSENTRY32 entry = {sizeof(entry)};
  if (!Process32First(snapshot.get(), &entry)) {
    ADD_FAILURE() << ErrorMessage("Process32First");
    return std::vector<pid_t>();
  }

  std::vector<pid_t> child_pids;
  do {
    if (entry.th32ParentProcessID == parent_pid)
      child_pids.push_back(entry.th32ProcessID);
  } while (Process32Next(snapshot.get(), &entry));

  return child_pids;
}

ULARGE_INTEGER GetProcessCreationTime(HANDLE process) {
  ULARGE_INTEGER ret = {};
  FILETIME creation_time;
  FILETIME dummy;
  if (GetProcessTimes(process, &creation_time, &dummy, &dummy, &dummy)) {
    ret.LowPart = creation_time.dwLowDateTime;
    ret.HighPart = creation_time.dwHighDateTime;
  } else {
    ADD_FAILURE() << ErrorMessage("GetProcessTimes");
  }

  return ret;
}

// Waits for the processes directly created by |parent| - and specifically not
// their offspring. For this to work without race, |parent| has to be suspended
// or have exited.
void WaitForAllChildProcessesOf(HANDLE parent) {
  pid_t parent_pid = GetProcessId(parent);
  std::vector<pid_t> child_pids = GetPotentialChildProcessesOf(parent_pid);

  ULARGE_INTEGER parent_creationtime = GetProcessCreationTime(parent);
  for (pid_t child_pid : child_pids) {
    // Try and open the process. This may fail for reasons such as:
    // 1. The process isn't |parent|'s child process, but rather a
    //    higher-privilege sub-process of an earlier process that had
    //    |parent|'s PID.
    // 2. The process no longer exists, e.g. it exited after enumeration.
    ScopedKernelHANDLE child_process(
        OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION | SYNCHRONIZE,
                    false,
                    child_pid));
    if (!child_process.is_valid())
      continue;

    // Check that the child now has the right parent PID, as its PID may have
    // been reused after the enumeration above.
    ProcessInfo child_info;
    if (!child_info.Initialize(child_process.get())) {
      // This can happen if child_process has exited after the handle is opened.
      LOG(ERROR) << "ProcessInfo::Initialize, pid: " << child_pid;
      continue;
    }

    if (parent_pid != child_info.ParentProcessID()) {
      // The child's process ID was reused after enumeration.
      continue;
    }

    // We successfully opened |child_process| and it has |parent|'s PID for
    // parent process ID. However, this could still be a sub-process of another
    // process that earlier had |parent|'s PID. To make sure, check that
    // |child_process| was created after |parent_process|.
    ULARGE_INTEGER process_creationtime =
        GetProcessCreationTime(child_process.get());
    if (process_creationtime.QuadPart < parent_creationtime.QuadPart)
      continue;

    DWORD err = WaitForSingleObject(child_process.get(), INFINITE);
    if (err == WAIT_FAILED) {
      ADD_FAILURE() << ErrorMessage("WaitForSingleObject");
    } else if (err != WAIT_OBJECT_0) {
      ADD_FAILURE() << "WaitForSingleObject returned " << err;
    }
  }
}

void StartAndUseHandler(const base::FilePath& temp_dir) {
  base::FilePath handler_path = Paths::Executable().DirName().Append(
      FILE_PATH_LITERAL("crashpad_handler.com"));

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

// Name of the environment variable used to communicate the name of the
// temp directory from parent to child process.
constexpr wchar_t kTempDirEnvName[] = L"CRASHPAD_TEST_TEMP_DIR";

class WinMultiprocessWithTempDir : public WinMultiprocess {
 public:
  WinMultiprocessWithTempDir()
      : WinMultiprocess(), temp_dir_env_(kTempDirEnvName) {}

  void WinMultiprocessParentBeforeChild() override {
    temp_dir_ = base::WrapUnique(new ScopedTempDir);
    temp_dir_env_.SetValue(temp_dir_->path().value().c_str());
  }

  void WinMultiprocessParentAfterChild(HANDLE child) override {
    WaitForAllChildProcessesOf(child);
    temp_dir_.reset();
  }

 protected:
  std::unique_ptr<ScopedTempDir> temp_dir_;
  ScopedEnvironmentVariable temp_dir_env_;

  DISALLOW_COPY_AND_ASSIGN(WinMultiprocessWithTempDir);
};

class StartWithInvalidHandles final : public WinMultiprocessWithTempDir {
 public:
  StartWithInvalidHandles() : WinMultiprocessWithTempDir() {}
  ~StartWithInvalidHandles() {}

 private:
  void WinMultiprocessParent() override {}

  void WinMultiprocessChild() override {
    HANDLE original_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE original_stderr = GetStdHandle(STD_ERROR_HANDLE);
    SetStdHandle(STD_OUTPUT_HANDLE, INVALID_HANDLE_VALUE);
    SetStdHandle(STD_ERROR_HANDLE, INVALID_HANDLE_VALUE);

    StartAndUseHandler(base::FilePath(temp_dir_env_.GetValue()));

    SetStdHandle(STD_OUTPUT_HANDLE, original_stdout);
    SetStdHandle(STD_ERROR_HANDLE, original_stderr);
  }
};

TEST(CrashpadClient, StartWithInvalidHandles) {
  WinMultiprocess::Run<StartWithInvalidHandles>();
}

class StartWithSameStdoutStderr final : public WinMultiprocessWithTempDir {
 public:
  StartWithSameStdoutStderr() : WinMultiprocessWithTempDir() {}
  ~StartWithSameStdoutStderr() {}

 private:
  void WinMultiprocessParent() override {}

  void WinMultiprocessChild() override {
    HANDLE original_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE original_stderr = GetStdHandle(STD_ERROR_HANDLE);
    SetStdHandle(STD_OUTPUT_HANDLE, original_stderr);

    StartAndUseHandler(base::FilePath(temp_dir_env_.GetValue()));

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
