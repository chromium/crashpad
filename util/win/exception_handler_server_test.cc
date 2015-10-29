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

#include "util/win/exception_handler_server.h"

#include <windows.h>

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/strings/stringprintf.h"
#include "client/crashpad_client.h"
#include "gtest/gtest.h"
#include "test/win/win_child_process.h"
#include "util/thread/thread.h"
#include "util/win/address_types.h"
#include "util/win/registration_protocol_win.h"
#include "util/win/scoped_handle.h"

namespace crashpad {
namespace test {
namespace {

// Runs the ExceptionHandlerServer on a background thread.
class RunServerThread : public Thread {
 public:
  // Instantiates a thread which will invoke server->Run(delegate).
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

class TestDelegate : public ExceptionHandlerServer::Delegate {
 public:
  explicit TestDelegate(HANDLE server_ready) : server_ready_(server_ready) {}
  ~TestDelegate() override {}

  void ExceptionHandlerServerStarted() override {
    SetEvent(server_ready_);
  }
  unsigned int ExceptionHandlerServerException(
      HANDLE process,
      WinVMAddress exception_information_address,
      WinVMAddress debug_critical_section_address) override {
    return 0;
  }

  void WaitForStart() { WaitForSingleObject(server_ready_, INFINITE); }

 private:
  HANDLE server_ready_;  // weak
  bool started_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

class ExceptionHandlerServerTest : public testing::Test {
 public:
  ExceptionHandlerServerTest()
      : pipe_name_("\\\\.\\pipe\\exception_handler_server_test_pipe_" +
                   base::StringPrintf("%08x", GetCurrentProcessId())),
        server_ready_(CreateEvent(nullptr, false, false, nullptr)),
        delegate_(server_ready_.get()),
        server_(pipe_name_),
        server_thread_(&server_, &delegate_) {}

  TestDelegate& delegate() { return delegate_; }
  ExceptionHandlerServer& server() { return server_; }
  Thread& server_thread() { return server_thread_; }
  const std::string& pipe_name() const { return pipe_name_; }

 private:
  std::string pipe_name_;
  ScopedKernelHANDLE server_ready_;
  TestDelegate delegate_;
  ExceptionHandlerServer server_;
  RunServerThread server_thread_;

  DISALLOW_COPY_AND_ASSIGN(ExceptionHandlerServerTest);
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

TEST_F(ExceptionHandlerServerTest, Instantiate) {
}

TEST_F(ExceptionHandlerServerTest, StartAndStop) {
  server_thread().Start();
  ScopedStopServerAndJoinThread scoped_stop_server_and_join_thread(
      &server(), &server_thread());
  ASSERT_NO_FATAL_FAILURE(delegate().WaitForStart());
}

TEST_F(ExceptionHandlerServerTest, StopWhileConnected) {
  server_thread().Start();
  ScopedStopServerAndJoinThread scoped_stop_server_and_join_thread(
      &server(), &server_thread());
  ASSERT_NO_FATAL_FAILURE(delegate().WaitForStart());
  CrashpadClient client;
  client.SetHandler(pipe_name());  // Connect to server.
  // Leaving this scope causes the server to be stopped, while the connection
  // is still open.
}

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

class TestClient final : public WinChildProcess {
 public:
  TestClient() : WinChildProcess() {}

  ~TestClient() {}

 private:
  int Run() override {
    std::string pipe_name = ReadString(ReadPipeHandle());
    CrashpadClient client;
    if (!client.SetHandler(pipe_name)) {
      ADD_FAILURE();
      return EXIT_FAILURE;
    }
    if (!client.UseHandler()) {
      ADD_FAILURE();
      return EXIT_FAILURE;
    }
    WriteString(WritePipeHandle(), "OK");
    return EXIT_SUCCESS;
  }

  DISALLOW_COPY_AND_ASSIGN(TestClient);
};

TEST_F(ExceptionHandlerServerTest, MultipleConnections) {
  WinChildProcess::EntryPoint<TestClient>();

  scoped_ptr<WinChildProcess::Handles> handles_1 = WinChildProcess::Launch();
  scoped_ptr<WinChildProcess::Handles> handles_2 = WinChildProcess::Launch();
  scoped_ptr<WinChildProcess::Handles> handles_3 = WinChildProcess::Launch();

  // Must ensure the delegate outlasts the server.
  {
    server_thread().Start();
    ScopedStopServerAndJoinThread scoped_stop_server_and_join_thread(
        &server(), &server_thread());
    ASSERT_NO_FATAL_FAILURE(delegate().WaitForStart());

    // Tell all the children where to connect.
    WriteString(handles_1->write.get(), pipe_name());
    WriteString(handles_2->write.get(), pipe_name());
    WriteString(handles_3->write.get(), pipe_name());

    ASSERT_EQ("OK", ReadString(handles_3->read.get()));
    ASSERT_EQ("OK", ReadString(handles_2->read.get()));
    ASSERT_EQ("OK", ReadString(handles_1->read.get()));
  }
}

}  // namespace
}  // namespace test
}  // namespace crashpad
