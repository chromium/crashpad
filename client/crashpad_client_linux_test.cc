// Copyright 2018 The Crashpad Authors. All rights reserved.
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

#include <stdlib.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/macros.h"
#include "client/crash_report_database.h"
#include "client/simulate_crash.h"
#include "gtest/gtest.h"
#include "test/errors.h"
#include "test/multiprocess_exec.h"
#include "test/scoped_temp_dir.h"
#include "test/test_paths.h"
#include "util/file/file_io.h"
#include "util/linux/exception_handler_client.h"
#include "util/linux/exception_information.h"
#include "util/misc/address_types.h"
#include "util/misc/from_pointer_cast.h"
#include "util/posix/signals.h"

namespace crashpad {
namespace test {
namespace {

TEST(CrashpadClient, SimulateCrash) {
  ScopedTempDir temp_dir;

  base::FilePath handler_path = TestPaths::Executable().DirName().Append(
      FILE_PATH_LITERAL("crashpad_handler"));

  crashpad::CrashpadClient client;
  ASSERT_TRUE(client.StartHandlerAtCrash(handler_path,
                             base::FilePath(temp_dir.path()),
                             base::FilePath(),
                             "",
                             std::map<std::string, std::string>(),
                             std::vector<std::string>()));

  CRASHPAD_SIMULATE_CRASH();

  auto database = CrashReportDatabase::BuildDatabase(temp_dir.path(), /* may_create= */ false);

  std::vector<CrashReportDatabase::Report> reports;
  ASSERT_EQ(database->GetPendingReports(&reports), CrashReportDatabase::kNoError);
  ASSERT_EQ(reports.size(), 0u);

  reports.clear();
  ASSERT_EQ(database->GetCompletedReports(&reports), CrashReportDatabase::kNoError);
  CHECK_EQ(reports.size(), 1u);
}

CRASHPAD_CHILD_TEST_MAIN(StartHandlerAtCrashChild) {
  FileHandle in = StdioFileHandle(StdioStream::kStandardInput);

  VMSize temp_dir_length;
  CheckedReadFileExactly(in, &temp_dir_length, sizeof(temp_dir_length));

  std::string temp_dir(temp_dir_length, '\0');
  CheckedReadFileExactly(in, &temp_dir[0], temp_dir_length);

  base::FilePath handler_path = TestPaths::Executable().DirName().Append(
      FILE_PATH_LITERAL("crashpad_handler"));

  crashpad::CrashpadClient client;
  if (!client.StartHandlerAtCrash(handler_path,
                             base::FilePath(temp_dir),
                             base::FilePath(),
                             "",
                             std::map<std::string, std::string>(),
                             std::vector<std::string>())) {
    return EXIT_FAILURE;
  }

  *(reinterpret_cast<volatile int*>(0)) = 42;

  NOTREACHED();
  return EXIT_SUCCESS;
}

class StartHandlerAtCrashTest : public MultiprocessExec {
 public:
  StartHandlerAtCrashTest() : MultiprocessExec() {
    SetChildTestMainFunction("StartHandlerAtCrashChild");
    SetExpectedChildTermination(kTerminationNormal, EXIT_FAILURE);
  }

 private:
  void MultiprocessParent() override {
    ScopedTempDir temp_dir;
    auto database = CrashReportDatabase::BuildDatabase(temp_dir.path(), /* may_create= */ true);

    std::vector<CrashReportDatabase::Report> reports;
    ASSERT_EQ(database->GetPendingReports(&reports), CrashReportDatabase::kNoError);
    EXPECT_EQ(reports.size(), 0u);

    ASSERT_EQ(database->GetCompletedReports(&reports), CrashReportDatabase::kNoError);
    EXPECT_EQ(reports.size(), 0u);

    VMSize temp_dir_length = temp_dir.path().value().size();
    ASSERT_TRUE(LoggingWriteFile(WritePipeHandle(), &temp_dir_length, sizeof(temp_dir_length)));

    ASSERT_TRUE(LoggingWriteFile(WritePipeHandle(), temp_dir.path().value().data(), temp_dir_length));

    CheckedReadFileAtEOF(ReadPipeHandle());

    ASSERT_EQ(database->GetPendingReports(&reports), CrashReportDatabase::kNoError);
    EXPECT_EQ(reports.size(), 0u);

    ASSERT_EQ(database->GetCompletedReports(&reports), CrashReportDatabase::kNoError);
    EXPECT_EQ(reports.size(), 1u);
  }

  DISALLOW_COPY_AND_ASSIGN(StartHandlerAtCrashTest);
};

TEST(CrashpadClient, StartHandlerAtCrash) {
  StartHandlerAtCrashTest test;
  test.Run();
}

// A signal handler that defers handler process startup to another, presumably
// more privileged, process.
class SandboxedHandler {
 public:
  static SandboxedHandler* Get() {
    static SandboxedHandler* instance = new SandboxedHandler();
    return instance;
  }

  bool Initialize(FileHandle client_sock) {
    client_sock_ = client_sock;
    return Signals::InstallCrashHandlers(HandleCrash, 0, nullptr);
  }

 private:
  SandboxedHandler() = default;
  ~SandboxedHandler() = delete;

  bool LaunchHandler() {
    char c;
    return LoggingWriteFile(client_sock_, &c, sizeof(c));
  }

  static void HandleCrash(int signo, siginfo_t* siginfo, void* context) {
    auto state = Get();

    CHECK(state->LaunchHandler());

    ExceptionInformation exception_information;
    exception_information.siginfo_address =
        FromPointerCast<decltype(exception_information.siginfo_address)>(
            siginfo);
    exception_information.context_address =
        FromPointerCast<decltype(exception_information.context_address)>(
            context);
    exception_information.thread_id = syscall(SYS_gettid);

    ClientInformation info = {};
    info.exception_information_address =
        FromPointerCast<decltype(info.exception_information_address)>(&exception_information);

    ExceptionHandlerClient handler_client(state->client_sock_);
    CHECK_EQ(handler_client.RequestCrashDump(info), 0);

    _exit(EXIT_SUCCESS);
  }

  FileHandle client_sock_;

  DISALLOW_COPY_AND_ASSIGN(SandboxedHandler);
};

class StartHandlerForClientTest {
 public:
  StartHandlerForClientTest() = default;
  ~StartHandlerForClientTest() = default;

  bool Initialize() {
    int socks[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, socks) != 0)  {
      PLOG(ERROR) << "socketpair";
      return false;
    }
    client_sock_.reset(socks[0]);
    server_sock_.reset(socks[1]);

    return true;
  }

  bool StartHandler() {
    base::FilePath handler_path = TestPaths::Executable().DirName().Append(
        FILE_PATH_LITERAL("crashpad_handler"));

    CrashpadClient client;
    return client.StartHandlerForClient(handler_path,
                                     temp_dir_.path(),
                                     base::FilePath(),
                                     "",
                                     std::map<std::string, std::string>(),
                                     std::vector<std::string>(),
                                     server_sock_.get());
  }

  void ExpectReport() {
    auto database = CrashReportDatabase::BuildDatabase(temp_dir_.path(), /* may_create= */ false);

    std::vector<CrashReportDatabase::Report> reports;

    ASSERT_EQ(database->GetPendingReports(&reports), CrashReportDatabase::kNoError);
    EXPECT_EQ(reports.size(), 0u);

    ASSERT_EQ(database->GetCompletedReports(&reports), CrashReportDatabase::kNoError);
    EXPECT_EQ(reports.size(), 1u);
  }


  FileHandle ClientSock() { return client_sock_.get(); }
  FileHandle ServerSock() { return server_sock_.get(); }
 private:
  ScopedTempDir temp_dir_;
  ScopedFileHandle client_sock_;
  ScopedFileHandle server_sock_;

  DISALLOW_COPY_AND_ASSIGN(StartHandlerForClientTest);
};

class StartHandlerForChildTest : public Multiprocess {
 public:
  StartHandlerForChildTest() : Multiprocess(), test_state_() {
    if (!test_state_.Initialize()) {
      ADD_FAILURE();
      return;
    }
  }

  ~StartHandlerForChildTest() = default;

 private:
  void MultiprocessParent() {
    char c;
    ASSERT_TRUE(LoggingReadFileExactly(test_state_.ServerSock(), &c, sizeof(c)));
    ASSERT_TRUE(test_state_.StartHandler());

    CheckedReadFileAtEOF(ReadPipeHandle());

    test_state_.ExpectReport();
  }

  void MultiprocessChild() {
    auto signal_handler = SandboxedHandler::Get();
    CHECK(signal_handler->Initialize(test_state_.ClientSock()));

    *(reinterpret_cast<volatile int*>(0)) = 42;

    NOTREACHED();
  }

  StartHandlerForClientTest test_state_;

  DISALLOW_COPY_AND_ASSIGN(StartHandlerForChildTest);
};

TEST(CrashpadClient, StartHandlerForChild) {
  StartHandlerForChildTest test;
  ASSERT_NO_FATAL_FAILURE();
  test.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
