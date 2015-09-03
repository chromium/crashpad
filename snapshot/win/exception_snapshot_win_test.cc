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

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "client/crashpad_client.h"
#include "gtest/gtest.h"
#include "snapshot/win/process_snapshot_win.h"
#include "test/win/win_child_process.h"
#include "util/thread/thread.h"
#include "util/win/exception_handler_server.h"
#include "util/win/registration_protocol_win.h"
#include "util/win/scoped_handle.h"

namespace crashpad {
namespace test {
namespace {

HANDLE DuplicateEvent(HANDLE process, HANDLE event) {
  HANDLE handle;
  if (DuplicateHandle(GetCurrentProcess(),
                      event,
                      process,
                      &handle,
                      SYNCHRONIZE | EVENT_MODIFY_STATE,
                      false,
                      0)) {
    return handle;
  }
  return nullptr;
}

class ExceptionSnapshotWinTest : public testing::Test {
 public:
  class Delegate : public ExceptionHandlerServer::Delegate {
   public:
    Delegate(HANDLE server_ready, HANDLE completed_test_event)
        : server_ready_(server_ready),
          completed_test_event_(completed_test_event),
          break_near_(nullptr) {}
    ~Delegate() override {}

    void set_break_near(void* break_near) { break_near_ = break_near; }

    void ExceptionHandlerServerStarted() override { SetEvent(server_ready_); }

    unsigned int ExceptionHandlerServerException(
        HANDLE process,
        WinVMAddress exception_information_address) override {
      ProcessSnapshotWin snapshot;
      snapshot.Initialize(process);
      snapshot.InitializeException(exception_information_address);

      // Confirm the exception record was read correctly.
      EXPECT_NE(snapshot.Exception()->ThreadID(), 0u);
      EXPECT_EQ(snapshot.Exception()->Exception(), EXCEPTION_BREAKPOINT);

      // Verify the exception happened at the expected location with a bit of
      // slop space to allow for reading the current PC before the exception
      // happens. See CrashingChildProcess::Run().
      const uint64_t kAllowedOffset = 64;
      EXPECT_GT(snapshot.Exception()->ExceptionAddress(),
                reinterpret_cast<uint64_t>(break_near_));
      EXPECT_LT(snapshot.Exception()->ExceptionAddress(),
                reinterpret_cast<uint64_t>(break_near_) + kAllowedOffset);

      SetEvent(completed_test_event_);

      return snapshot.Exception()->Exception();
    }

   private:
    HANDLE server_ready_;  // weak
    HANDLE completed_test_event_;  // weak
    void* break_near_;

    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

 private:
  ScopedKernelHANDLE exception_happened_;
};

// Runs the ExceptionHandlerServer on a background thread.
class RunServerThread : public Thread {
 public:
  // Instantiates a thread which will invoke server->Run(delegate, pipe_name);
  RunServerThread(ExceptionHandlerServer* server,
                  ExceptionHandlerServer::Delegate* delegate,
                  const std::string& pipe_name)
      : server_(server), delegate_(delegate), pipe_name_(pipe_name) {}
  ~RunServerThread() override {}

 private:
  // Thread:
  void ThreadMain() override { server_->Run(delegate_, pipe_name_); }

  ExceptionHandlerServer* server_;
  ExceptionHandlerServer::Delegate* delegate_;
  std::string pipe_name_;

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

std::string ReadString(FileHandle handle) {
  size_t length = 0;
  EXPECT_TRUE(LoggingReadFile(handle, &length, sizeof(length)));
  scoped_ptr<char[]> buffer(new char[length]);
  EXPECT_TRUE(LoggingReadFile(handle, &buffer[0], length));
  return std::string(&buffer[0], length);
}

void WriteString(FileHandle handle, const std::string& str) {
  size_t length = str.size();
  EXPECT_TRUE(LoggingWriteFile(handle, &length, sizeof(length)));
  EXPECT_TRUE(LoggingWriteFile(handle, &str[0], length));
}

__declspec(noinline) void* CurrentAddress() {
  return _ReturnAddress();
}

class CrashingChildProcess final : public WinChildProcess {
 public:
  CrashingChildProcess() : WinChildProcess() {}
  ~CrashingChildProcess() {}

 private:
  int Run() override {
    std::string pipe_name = ReadString(ReadPipeHandle());
    CrashpadClient client;
    EXPECT_TRUE(client.SetHandler(pipe_name));
    EXPECT_TRUE(client.UseHandler());
    // Save the address where we're about to crash so the exception handler can
    // verify it's in approximately the right location (with a bit of fudge for
    // the code between here and the __debugbreak()).
    void* break_address = CurrentAddress();
    LoggingWriteFile(WritePipeHandle(), &break_address, sizeof(break_address));
    __debugbreak();
    return 0;
  };
};

TEST_F(ExceptionSnapshotWinTest, ChildCrash) {
  // Spawn a child process that will immediately crash (once we let it
  // run below by telling it what to connect to).
  WinChildProcess::EntryPoint<CrashingChildProcess>();
  scoped_ptr<WinChildProcess::Handles> handle = WinChildProcess::Launch();

  // Set up the registration server on a background thread.
  std::string pipe_name = "\\\\.\\pipe\\handler_test_pipe_" +
                          base::StringPrintf("%08x", GetCurrentProcessId());
  ScopedKernelHANDLE server_ready(CreateEvent(nullptr, false, false, nullptr));
  ScopedKernelHANDLE completed(CreateEvent(nullptr, false, false, nullptr));
  Delegate delegate(server_ready.get(), completed.get());

  ExceptionHandlerServer exception_handler_server;
  RunServerThread server_thread(
      &exception_handler_server, &delegate, pipe_name);
  server_thread.Start();
  ScopedStopServerAndJoinThread scoped_stop_server_and_join_thread(
      &exception_handler_server, &server_thread);

  WaitForSingleObject(server_ready.get(), INFINITE);
  // Allow the child to continue and tell it where to connect to.
  WriteString(handle->write.get(), pipe_name);

  // The child tells us (approximately) where it will crash.
  void* break_near_address;
  LoggingReadFile(
      handle->read.get(), &break_near_address, sizeof(break_near_address));
  delegate.set_break_near(break_near_address);

  // Wait for the child to crash and the exception information to be validated.
  WaitForSingleObject(completed.get(), INFINITE);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
