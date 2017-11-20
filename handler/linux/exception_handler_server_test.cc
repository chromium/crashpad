// Copyright 2017 The Crashpad Authors. All rights reserved.
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

#include "handler/linux/exception_handler_server.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/macros.h"
#include "gtest/gtest.h"
#include "test/errors.h"
#include "util/file/file_io.h"
#include "util/linux/registration_protocol.h"
#include "util/synchronization/semaphore.h"
#include "util/thread/thread.h"

namespace crashpad {
namespace test {
namespace {

// Runs the ExceptionHandlerServer on a background thread.
class RunServerThread : public Thread {
 public:
  // Instantiates a thread which will invoke server->Run(delegate).
  RunServerThread(ExceptionHandlerServer* server,
                  ExceptionHandlerServer::Delegate* delegate)
      : server_(server), delegate_(delegate), join_sem_(0) {}
  ~RunServerThread() override {}

  bool JoinWithTimeout(double timeout) {
    if (!join_sem_.TimedWait(timeout)) {
      return false;
    }
    Join();
    return true;
  }

 private:
  // Thread:
  void ThreadMain() override {
    server_->Run(delegate_);
    LOG(INFO) << "Server exiting";
    join_sem_.Signal();
  }

  ExceptionHandlerServer* server_;
  ExceptionHandlerServer::Delegate* delegate_;
  Semaphore join_sem_;

  DISALLOW_COPY_AND_ASSIGN(RunServerThread);
};

class TestDelegate : public ExceptionHandlerServer::Delegate {
 public:
  TestDelegate() : Delegate(), last_exception_address_(0), last_client_(-1), last_broker_(-1), sem_(0) {}

  ~TestDelegate() override {}

  bool WaitForException(double timeout_seconds,
                        pid_t* last_client,
                        LinuxVMAddress* last_address,
                        int* last_broker) {
    if (sem_.TimedWait(timeout_seconds)) {
      *last_client = last_client_;
      *last_address = last_exception_address_;
      *last_broker = last_broker_;
      return true;
    }
    return false;
  }

  void HandleException(pid_t client_process_id,
                       LinuxVMAddress exception_information_address) override {
    last_exception_address_ = exception_information_address;
    last_client_ = client_process_id;
    last_broker_ = -1;
    sem_.Signal();
  }

  void HandleExceptionWithBroker(
      pid_t client_process_id,
      LinuxVMAddress exception_information_address,
      int broker_sock) override {
    last_exception_address_ = exception_information_address,
    last_client_ = client_process_id;
    last_broker_ = broker_sock;
    sem_.Signal();
  }

 private:
  LinuxVMAddress last_exception_address_;
  pid_t last_client_;
  int last_broker_;
  Semaphore sem_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

class ExceptionHandlerServerTest : public testing::Test {
 public:
  ExceptionHandlerServerTest()
      : server_(),
        delegate_(),
        server_thread_(&server_, &delegate_),
        sock_to_handler_() {}

  int SockToHandler() { return sock_to_handler_.get(); }
  TestDelegate* Delegate() { return &delegate_; }
  void Hangup() { sock_to_handler_.reset(); }

  RunServerThread* ServerThread() { return &server_thread_; }
  ExceptionHandlerServer* Server() { return &server_; }

  ~ExceptionHandlerServerTest() = default;

 protected:
  void SetUp() override {
    int socks[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, socks), 0);
    sock_to_handler_.reset(socks[0]);

    Registration registration;
    registration.exception_information_address = 0;
    registration.client_process_id = getpid();
    registration.use_broker = false;
    ASSERT_TRUE(
        server_.InitializeWithClient(registration, ScopedFileHandle(socks[1])));
  }

 private:
  ExceptionHandlerServer server_;
  TestDelegate delegate_;
  RunServerThread server_thread_;
  ScopedFileHandle sock_to_handler_;
};

TEST_F(ExceptionHandlerServerTest, ShutdownWithNoClients) {
  ServerThread()->Start();
  Hangup();
  ASSERT_TRUE(ServerThread()->JoinWithTimeout(5.0));
}

TEST_F(ExceptionHandlerServerTest, StopWithClients) {
  ServerThread()->Start();
  Server()->Stop();
  ASSERT_TRUE(ServerThread()->JoinWithTimeout(5.0));
}

TEST_F(ExceptionHandlerServerTest, StopBeforeRun) {
  Server()->Stop();
  ServerThread()->Start();
  ASSERT_TRUE(ServerThread()->JoinWithTimeout(5.0));
}

TEST_F(ExceptionHandlerServerTest, MultipleStops) {
  ServerThread()->Start();
  Server()->Stop();
  Server()->Stop();
  ASSERT_TRUE(ServerThread()->JoinWithTimeout(5.0));
}

class ScopedStopServerAndJoinThread {
 public:
  ScopedStopServerAndJoinThread(ExceptionHandlerServer* server,
                                RunServerThread* thread)
      : server_(server), thread_(thread) {}
  ~ScopedStopServerAndJoinThread() {
    server_->Stop();
    EXPECT_TRUE(thread_->JoinWithTimeout(5.0));
  }

 private:
  ExceptionHandlerServer* server_;
  RunServerThread* thread_;
  DISALLOW_COPY_AND_ASSIGN(ScopedStopServerAndJoinThread);
};

TEST_F(ExceptionHandlerServerTest, Registration) {
  ScopedStopServerAndJoinThread stop_server(Server(), ServerThread());
  ServerThread()->Start();

  int reg_socks[2];
  ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, reg_socks), 0);
  ScopedFileHandle reg_client_sock(reg_socks[0]);
  ScopedFileHandle reg_server_sock(reg_socks[1]);

  Registration registration;
  registration.exception_information_address = 0;
  registration.client_process_id = getpid();
  registration.use_broker = false;

  ASSERT_TRUE(RegisterWithHandler(
      SockToHandler(), registration, reg_server_sock.get()));
  reg_server_sock.reset();

  ASSERT_TRUE(RequestCrashDump(reg_client_sock.get()));

  LinuxVMAddress last_address;
  pid_t last_client;
  int last_broker;
  ASSERT_TRUE(
      Delegate()->WaitForException(5.0, &last_client, &last_address, &last_broker));
  EXPECT_EQ(last_address, registration.exception_information_address);
  EXPECT_EQ(last_client, registration.client_process_id);
  EXPECT_EQ(last_broker, -1);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
