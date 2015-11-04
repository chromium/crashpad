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

#include "snapshot/win/exception_snapshot_win.h"

#include <string>

#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "client/crashpad_client.h"
#include "gtest/gtest.h"
#include "snapshot/win/process_snapshot_win.h"
#include "test/paths.h"
#include "test/win/child_launcher.h"
#include "util/file/file_io.h"
#include "util/thread/thread.h"
#include "util/win/exception_handler_server.h"
#include "util/win/registration_protocol_win.h"
#include "util/win/scoped_handle.h"
#include "util/win/scoped_process_suspend.h"

namespace crashpad {
namespace test {
namespace {

// Runs the ExceptionHandlerServer on a background thread.
class RunServerThread : public Thread {
 public:
  // Instantiates a thread which will invoke server->Run(delegate);
  RunServerThread(ExceptionHandlerServer* server,
                  ExceptionHandlerServer::Delegate* delegate)
      : server_(server), delegate_(delegate) {}
  ~RunServerThread() override {}

 private:
  // Thread:
  void ThreadMain() override { server_->Run(delegate_); }

  ExceptionHandlerServer* server_;
  ExceptionHandlerServer::Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(RunServerThread);
};

// During destruction, ensures that the server is stopped and the background
// thread joined.
class ScopedStopServerAndJoinThread {
 public:
  ScopedStopServerAndJoinThread(ExceptionHandlerServer* server, Thread* thread)
      : server_(server), thread_(thread) {}
  ~ScopedStopServerAndJoinThread() {
    server_->Stop();
    thread_->Join();
  }

 private:
  ExceptionHandlerServer* server_;
  Thread* thread_;
  DISALLOW_COPY_AND_ASSIGN(ScopedStopServerAndJoinThread);
};

class CrashingDelegate : public ExceptionHandlerServer::Delegate {
 public:
  CrashingDelegate(HANDLE server_ready, HANDLE completed_test_event)
      : server_ready_(server_ready),
        completed_test_event_(completed_test_event),
        break_near_(0) {}
  ~CrashingDelegate() override {}

  void set_break_near(WinVMAddress break_near) { break_near_ = break_near; }

  void ExceptionHandlerServerStarted() override { SetEvent(server_ready_); }

  unsigned int ExceptionHandlerServerException(
      HANDLE process,
      WinVMAddress exception_information_address,
      WinVMAddress debug_critical_section_address) override {
    ScopedProcessSuspend suspend(process);
    ProcessSnapshotWin snapshot;
    snapshot.Initialize(process,
                        ProcessSuspensionState::kSuspended,
                        debug_critical_section_address);
    snapshot.InitializeException(exception_information_address);

    // Confirm the exception record was read correctly.
    EXPECT_NE(snapshot.Exception()->ThreadID(), 0u);
    EXPECT_EQ(snapshot.Exception()->Exception(), EXCEPTION_BREAKPOINT);

    // Verify the exception happened at the expected location with a bit of
    // slop space to allow for reading the current PC before the exception
    // happens. See TestCrashingChild().
    const uint64_t kAllowedOffset = 64;
    EXPECT_GT(snapshot.Exception()->ExceptionAddress(), break_near_);
    EXPECT_LT(snapshot.Exception()->ExceptionAddress(),
              break_near_ + kAllowedOffset);

    SetEvent(completed_test_event_);

    return snapshot.Exception()->Exception();
  }

 private:
  HANDLE server_ready_;  // weak
  HANDLE completed_test_event_;  // weak
  WinVMAddress break_near_;

  DISALLOW_COPY_AND_ASSIGN(CrashingDelegate);
};

void TestCrashingChild(const base::string16& directory_modification) {
  // Set up the registration server on a background thread.
  ScopedKernelHANDLE server_ready(CreateEvent(nullptr, false, false, nullptr));
  ScopedKernelHANDLE completed(CreateEvent(nullptr, false, false, nullptr));
  CrashingDelegate delegate(server_ready.get(), completed.get());

  ExceptionHandlerServer exception_handler_server(true);
  std::wstring pipe_name = exception_handler_server.CreatePipe();
  RunServerThread server_thread(&exception_handler_server, &delegate);
  server_thread.Start();
  ScopedStopServerAndJoinThread scoped_stop_server_and_join_thread(
      &exception_handler_server, &server_thread);

  WaitForSingleObject(server_ready.get(), INFINITE);

  // Spawn a child process, passing it the pipe name to connect to.
  base::FilePath test_executable = Paths::Executable();
  std::wstring child_test_executable =
      test_executable.DirName()
          .Append(directory_modification)
          .Append(test_executable.BaseName().RemoveFinalExtension().value() +
                  L"_crashing_child.exe")
          .value();
  ChildLauncher child(child_test_executable, pipe_name);
  child.Start();

  // The child tells us (approximately) where it will crash.
  WinVMAddress break_near_address;
  LoggingReadFile(child.stdout_read_handle(),
                  &break_near_address,
                  sizeof(break_near_address));
  delegate.set_break_near(break_near_address);

  // Wait for the child to crash and the exception information to be validated.
  WaitForSingleObject(completed.get(), INFINITE);
}

TEST(ExceptionSnapshotWinTest, ChildCrash) {
  TestCrashingChild(FILE_PATH_LITERAL("."));
}

#if defined(ARCH_CPU_64_BITS)
TEST(ExceptionSnapshotWinTest, ChildCrashWOW64) {
#ifndef NDEBUG
  TestCrashingChild(FILE_PATH_LITERAL("..\\..\\out\\Debug"));
#else
  TestCrashingChild(FILE_PATH_LITERAL("..\\..\\out\\Release"));
#endif
}
#endif  // ARCH_CPU_64_BITS

class SimulateDelegate : public ExceptionHandlerServer::Delegate {
 public:
  SimulateDelegate(HANDLE server_ready, HANDLE completed_test_event)
      : server_ready_(server_ready),
        completed_test_event_(completed_test_event),
        dump_near_(0) {}
  ~SimulateDelegate() override {}

  void set_dump_near(WinVMAddress dump_near) { dump_near_ = dump_near; }

  void ExceptionHandlerServerStarted() override { SetEvent(server_ready_); }

  unsigned int ExceptionHandlerServerException(
      HANDLE process,
      WinVMAddress exception_information_address,
      WinVMAddress debug_critical_section_address) override {
    ScopedProcessSuspend suspend(process);
    ProcessSnapshotWin snapshot;
    snapshot.Initialize(process,
                        ProcessSuspensionState::kSuspended,
                        debug_critical_section_address);
    snapshot.InitializeException(exception_information_address);
    EXPECT_TRUE(snapshot.Exception());
    EXPECT_EQ(0x517a7ed, snapshot.Exception()->Exception());

    // Verify the dump was captured at the expected location with some slop
    // space.
    const uint64_t kAllowedOffset = 64;
    EXPECT_GT(snapshot.Exception()->Context()->InstructionPointer(),
              dump_near_);
    EXPECT_LT(snapshot.Exception()->Context()->InstructionPointer(),
              dump_near_ + kAllowedOffset);

    EXPECT_EQ(snapshot.Exception()->Context()->InstructionPointer(),
              snapshot.Exception()->ExceptionAddress());

    SetEvent(completed_test_event_);

    return 0;
  }

 private:
  HANDLE server_ready_;  // weak
  HANDLE completed_test_event_;  // weak
  WinVMAddress dump_near_;

  DISALLOW_COPY_AND_ASSIGN(SimulateDelegate);
};

void TestDumpWithoutCrashingChild(
    const base::string16& directory_modification) {
  // Set up the registration server on a background thread.
  ScopedKernelHANDLE server_ready(CreateEvent(nullptr, false, false, nullptr));
  ScopedKernelHANDLE completed(CreateEvent(nullptr, false, false, nullptr));
  SimulateDelegate delegate(server_ready.get(), completed.get());

  ExceptionHandlerServer exception_handler_server(true);
  std::wstring pipe_name = exception_handler_server.CreatePipe();
  RunServerThread server_thread(&exception_handler_server, &delegate);
  server_thread.Start();
  ScopedStopServerAndJoinThread scoped_stop_server_and_join_thread(
      &exception_handler_server, &server_thread);

  WaitForSingleObject(server_ready.get(), INFINITE);

  // Spawn a child process, passing it the pipe name to connect to.
  base::FilePath test_executable = Paths::Executable();
  std::wstring child_test_executable =
      test_executable.DirName()
          .Append(directory_modification)
          .Append(test_executable.BaseName().RemoveFinalExtension().value() +
                  L"_dump_without_crashing.exe")
          .value();
  ChildLauncher child(child_test_executable, pipe_name);
  child.Start();

  // The child tells us (approximately) where it will capture a dump.
  WinVMAddress dump_near_address;
  LoggingReadFile(child.stdout_read_handle(),
                  &dump_near_address,
                  sizeof(dump_near_address));
  delegate.set_dump_near(dump_near_address);

  // Wait for the child to crash and the exception information to be validated.
  WaitForSingleObject(completed.get(), INFINITE);
}

TEST(SimulateCrash, ChildDumpWithoutCrashing) {
  TestDumpWithoutCrashingChild(FILE_PATH_LITERAL("."));
}

#if defined(ARCH_CPU_64_BITS)
TEST(SimulateCrash, ChildDumpWithoutCrashingWOW64) {
#ifndef NDEBUG
  TestDumpWithoutCrashingChild(FILE_PATH_LITERAL("..\\..\\out\\Debug"));
#else
  TestDumpWithoutCrashingChild(FILE_PATH_LITERAL("..\\..\\out\\Release"));
#endif
}
#endif  // ARCH_CPU_64_BITS

}  // namespace
}  // namespace test
}  // namespace crashpad
