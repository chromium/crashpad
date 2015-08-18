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
#include "client/crashpad_info.h"
#include "handler/win/registration_server.h"
#include "gtest/gtest.h"
#include "snapshot/win/process_reader_win.h"
#include "snapshot/win/process_snapshot_win.h"
#include "test/win/win_child_process.h"
#include "util/thread/thread.h"
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
  class Delegate : public RegistrationServer::Delegate {
   public:
    Delegate()
        : crashpad_info_address_(0),
          client_process_(),
          started_event_(CreateEvent(nullptr, false, false, nullptr)),
          request_dump_event_(CreateEvent(nullptr, false, false, nullptr)),
          dump_complete_event_(CreateEvent(nullptr, true, false, nullptr)) {
      EXPECT_TRUE(started_event_.is_valid());
      EXPECT_TRUE(request_dump_event_.is_valid());
      EXPECT_TRUE(dump_complete_event_.is_valid());
    }

    ~Delegate() override {
    }

    void OnStarted() override {
      EXPECT_EQ(WAIT_TIMEOUT, WaitForSingleObject(started_event_.get(), 0));
      SetEvent(started_event_.get());
    }

    bool RegisterClient(ScopedKernelHANDLE client_process,
                        WinVMAddress crashpad_info_address,
                        HANDLE* request_dump_event,
                        HANDLE* dump_complete_event) override {
      client_process_ = client_process.Pass();
      crashpad_info_address_ = crashpad_info_address;
      *request_dump_event =
          DuplicateEvent(client_process_.get(), request_dump_event_.get());
      *dump_complete_event =
          DuplicateEvent(client_process_.get(), dump_complete_event_.get());
      return true;
    }

    void WaitForStart() {
      DWORD wait_result = WaitForSingleObject(started_event_.get(), INFINITE);
      if (wait_result == WAIT_FAILED)
        PLOG(ERROR);
      ASSERT_EQ(wait_result, WAIT_OBJECT_0);
    }

    void WaitForDumpRequestAndValidateException(void* break_near) {
      // Wait until triggered, and then grab information from the child.
      WaitForSingleObject(request_dump_event_.get(), INFINITE);

      // Snapshot the process and exception.
      ProcessReaderWin process_reader;
      ASSERT_TRUE(process_reader.Initialize(client_process_.get()));
      CrashpadInfo crashpad_info;
      ASSERT_TRUE(process_reader.ReadMemory(
          crashpad_info_address_, sizeof(crashpad_info), &crashpad_info));
      ProcessSnapshotWin snapshot;
      snapshot.Initialize(client_process_.get());
      snapshot.InitializeException(
          crashpad_info.thread_id(),
          reinterpret_cast<WinVMAddress>(crashpad_info.exception_pointers()));

      // Confirm the exception record was read correctly.
      EXPECT_NE(snapshot.Exception()->ThreadID(), 0u);
      EXPECT_EQ(snapshot.Exception()->Exception(), EXCEPTION_BREAKPOINT);

      // Verify the exception happened at the expected location with a bit of
      // slop space to allow for reading the current PC before the exception
      // happens. See CrashingChildProcess::Run().
      const uint64_t kAllowedOffset = 64;
      EXPECT_GT(snapshot.Exception()->ExceptionAddress(),
                reinterpret_cast<uint64_t>(break_near));
      EXPECT_LT(snapshot.Exception()->ExceptionAddress(),
                reinterpret_cast<uint64_t>(break_near) + kAllowedOffset);

      // Notify the child that we're done.
      SetEvent(dump_complete_event_.get());
    }

    ScopedKernelHANDLE* request_dump_event() { return &request_dump_event_; }
    ScopedKernelHANDLE* dump_complete_event() { return &dump_complete_event_; }

   private:
    WinVMAddress crashpad_info_address_;
    ScopedKernelHANDLE client_process_;
    ScopedKernelHANDLE started_event_;
    ScopedKernelHANDLE request_dump_event_;
    ScopedKernelHANDLE dump_complete_event_;
  };
};

// Runs the RegistrationServer on a background thread.
class RunServerThread : public Thread {
 public:
  // Instantiates a thread which will invoke server->Run(pipe_name, delegate).
  RunServerThread(RegistrationServer* server,
                  const base::string16& pipe_name,
                  RegistrationServer::Delegate* delegate)
      : server_(server), pipe_name_(pipe_name), delegate_(delegate) {}
  ~RunServerThread() override {}

 private:
  // Thread:
  void ThreadMain() override { server_->Run(pipe_name_, delegate_); }

  RegistrationServer* server_;
  base::string16 pipe_name_;
  RegistrationServer::Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(RunServerThread);
};

// During destruction, ensures that the server is stopped and the background
// thread joined.
class ScopedStopServerAndJoinThread {
 public:
  explicit ScopedStopServerAndJoinThread(RegistrationServer* server,
                                         Thread* thread)
      : server_(server), thread_(thread) {}
  ~ScopedStopServerAndJoinThread() {
    server_->Stop();
    thread_->Join();
  }

 private:
  RegistrationServer* server_;
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
  // Set up the registration server on a background thread.
  RegistrationServer server;
  std::string pipe_name = "\\\\.\\pipe\\handler_test_pipe_" +
                          base::StringPrintf("%08x", GetCurrentProcessId());
  base::string16 pipe_name_16 = base::UTF8ToUTF16(pipe_name);
  Delegate delegate;
  RunServerThread server_thread(&server, pipe_name_16, &delegate);
  server_thread.Start();
  ScopedStopServerAndJoinThread scoped_stop_server_and_join_thread(
      &server, &server_thread);
  ASSERT_NO_FATAL_FAILURE(delegate.WaitForStart());

  // Spawn a child process that immediately crashes.
  WinChildProcess::EntryPoint<CrashingChildProcess>();
  scoped_ptr<WinChildProcess::Handles> handle = WinChildProcess::Launch();
  WriteString(handle->write.get(), pipe_name);

  void* break_near_address;
  LoggingReadFile(
      handle->read.get(), &break_near_address, sizeof(break_near_address));

  // Verify the exception information is as expected.
  delegate.WaitForDumpRequestAndValidateException(break_near_address);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
