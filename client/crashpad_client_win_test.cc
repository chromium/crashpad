// Copyright 2015 The Crashpad Authors
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

#include <vector>

#include "base/files/file_path.h"
#include "client/crash_report_database.h"
#include "gtest/gtest.h"
#include "test/test_paths.h"
#include "test/scoped_temp_dir.h"
#include "test/win/win_multiprocess.h"
#include "test/win/win_multiprocess_with_temp_dir.h"
#include "util/win/scoped_handle.h"
#include "util/win/termination_codes.h"

namespace crashpad {
namespace test {
namespace {

void StartAndUseHandler(CrashpadClient& client, const base::FilePath& temp_dir) {
  base::FilePath handler_path = TestPaths::Executable().DirName().Append(
      FILE_PATH_LITERAL("crashpad_handler.com"));

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

void StartAndUseHandler(const base::FilePath& temp_dir) {
  CrashpadClient client;
  StartAndUseHandler(client, temp_dir);
}

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

    StartAndUseHandler(GetTempDirPath());

    SetStdHandle(STD_OUTPUT_HANDLE, original_stdout);
    SetStdHandle(STD_ERROR_HANDLE, original_stderr);
  }
};

TEST(CrashpadClient, StartWithInvalidHandles) {
  WinMultiprocessWithTempDir::Run<StartWithInvalidHandles>();
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

    StartAndUseHandler(GetTempDirPath());

    SetStdHandle(STD_OUTPUT_HANDLE, original_stdout);
  }
};

TEST(CrashpadClient, StartWithSameStdoutStderr) {
  WinMultiprocessWithTempDir::Run<StartWithSameStdoutStderr>();
}

void StartAndUseBrokenHandler(CrashpadClient* client) {
  ScopedTempDir temp_dir;
  base::FilePath handler_path = TestPaths::Executable().DirName().Append(
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

#if defined(ADDRESS_SANITIZER)
// https://crbug.com/845011
#define MAYBE_HandlerLaunchFailureCrash DISABLED_HandlerLaunchFailureCrash
#else
#define MAYBE_HandlerLaunchFailureCrash HandlerLaunchFailureCrash
#endif
TEST(CrashpadClient, MAYBE_HandlerLaunchFailureCrash) {
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

#if defined(ADDRESS_SANITIZER)
// https://crbug.com/845011
#define MAYBE_HandlerLaunchFailureDumpAndCrash \
  DISABLED_HandlerLaunchFailureDumpAndCrash
#else
#define MAYBE_HandlerLaunchFailureDumpAndCrash HandlerLaunchFailureDumpAndCrash
#endif
TEST(CrashpadClient, MAYBE_HandlerLaunchFailureDumpAndCrash) {
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

class NoDumpExpected : public WinMultiprocessWithTempDir {
 private:
  void WinMultiprocessParentAfterChild(HANDLE child) override {
    // Make sure no dump was generated.
    std::unique_ptr<CrashReportDatabase> database(
        CrashReportDatabase::Initialize(GetTempDirPath()));
    ASSERT_TRUE(database);

    std::vector<CrashReportDatabase::Report> reports;
    ASSERT_EQ(database->GetPendingReports(&reports),
              CrashReportDatabase::kNoError);
    ASSERT_EQ(reports.size(), 0u);
  }
};

// Crashing the process under test does not result in a crashed status as an
// exit code in debug builds, so we only verify this behavior in release
// builds.
#if defined(NDEBUG)
class CrashWithDisabledGlobalHooks final : public NoDumpExpected {
 public:
  CrashWithDisabledGlobalHooks() : NoDumpExpected() {}
  ~CrashWithDisabledGlobalHooks() {}

 private:
  void WinMultiprocessParent() override {
    SetExpectedChildExitCode(STATUS_ACCESS_VIOLATION);
  }

  void WinMultiprocessChild() override {
    CrashpadClient client;
    client.DisableGlobalHooks();
    StartAndUseHandler(client, GetTempDirPath());
    int* bad = nullptr;
    *bad = 1;
  }
};

TEST(CrashpadClient, CrashWithDisabledGlobalHooks) {
  WinMultiprocessWithTempDir::Run<CrashWithDisabledGlobalHooks>();
}
#endif  // defined(NDEBUG)

class DumpAndCrashWithDisabledGlobalHooks final
    : public WinMultiprocessWithTempDir {
 public:
  DumpAndCrashWithDisabledGlobalHooks() : WinMultiprocessWithTempDir() {}
  ~DumpAndCrashWithDisabledGlobalHooks() {}

 private:
  static constexpr DWORD kExpectedExitCode = 0x1CEB00DA;

  void WinMultiprocessParent() override {
    SetExpectedChildExitCode(kExpectedExitCode);
  }

  void WinMultiprocessChild() override {
    CrashpadClient client;
    client.DisableGlobalHooks();
    StartAndUseHandler(client, GetTempDirPath());
    EXCEPTION_RECORD exception_record = {kExpectedExitCode,
                                         EXCEPTION_NONCONTINUABLE};
    CONTEXT context;
    CaptureContext(&context);
    EXCEPTION_POINTERS exception_pointers = {&exception_record, &context};
    CrashpadClient::DumpAndCrash(&exception_pointers);
  }

  void WinMultiprocessParentAfterChild(HANDLE child) override {
    // Make sure the dump was generated.
    std::unique_ptr<CrashReportDatabase> database(
        CrashReportDatabase::Initialize(GetTempDirPath()));
    ASSERT_TRUE(database);

    std::vector<CrashReportDatabase::Report> reports;
    ASSERT_EQ(database->GetPendingReports(&reports),
              CrashReportDatabase::kNoError);
    ASSERT_EQ(reports.size(), 1u);

    // Delegate the cleanup to the superclass.
    WinMultiprocessWithTempDir::WinMultiprocessParentAfterChild(child);
  }
};

TEST(CrashpadClient, DumpAndCrashWithDisabledGlobalHooks) {
  WinMultiprocessWithTempDir::Run<DumpAndCrashWithDisabledGlobalHooks>();
}

#if !defined(ADDRESS_SANITIZER)
class HeapCorruptionWithDisabledGlobalHooks final : public NoDumpExpected {
 public:
  HeapCorruptionWithDisabledGlobalHooks() : NoDumpExpected() {}
  ~HeapCorruptionWithDisabledGlobalHooks() {}

 private:
  void WinMultiprocessParent() override {
    SetExpectedChildExitCode(STATUS_HEAP_CORRUPTION);
  }

  void WinMultiprocessChild() override {
    CrashpadClient client;
    client.DisableGlobalHooks();
    StartAndUseHandler(client, GetTempDirPath());
    int* bad = reinterpret_cast<int*>(1);
    delete bad;
  }
};

TEST(CrashpadClient, HeapCorruptionWithDisabledGlobalHooks) {
  WinMultiprocessWithTempDir::Run<HeapCorruptionWithDisabledGlobalHooks>();
}

#endif  // !defined(ADDRESS_SANITIZER)

}  // namespace
}  // namespace test
}  // namespace crashpad
