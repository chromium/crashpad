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

#include "handler/win/registration_server.h"

#include <windows.h>

#include <vector>

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "client/crashpad_info.h"
#include "client/registration_protocol_win.h"
#include "gtest/gtest.h"
#include "handler/win/registration_test_base.h"
#include "util/thread/thread.h"
#include "util/win/address_types.h"
#include "util/win/scoped_handle.h"

namespace crashpad {
namespace test {
namespace {

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

class RegistrationServerTest : public RegistrationTestBase {
 public:
  RegistrationServerTest()
      : server_(), server_thread_(&server_, pipe_name(), &delegate()) {}

  RegistrationServer& server() { return server_; }
  Thread& server_thread() { return server_thread_; }

 private:
  RegistrationServer server_;
  RunServerThread server_thread_;

  DISALLOW_COPY_AND_ASSIGN(RegistrationServerTest);
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

TEST_F(RegistrationServerTest, Instantiate) {
}

TEST_F(RegistrationServerTest, StartAndStop) {
  server_thread().Start();
  ScopedStopServerAndJoinThread scoped_stop_server_and_join_thread(
      &server(), &server_thread());
  ASSERT_NO_FATAL_FAILURE(delegate().WaitForStart());
}

TEST_F(RegistrationServerTest, StopWhileConnected) {
  ScopedFileHANDLE connection;
  {
    server_thread().Start();
    ScopedStopServerAndJoinThread scoped_stop_server_and_join_thread(
        &server(), &server_thread());
    ASSERT_NO_FATAL_FAILURE(delegate().WaitForStart());
    connection = Connect();
    ASSERT_TRUE(connection.is_valid());
    // Leaving this scope causes the server to be stopped, while the connection
    // is still open.
  }
}

TEST_F(RegistrationServerTest, Register) {
  RegistrationRequest request = {0};
  RegistrationResponse response = {0};
  CrashpadInfo crashpad_info;
  request.client_process_id = GetCurrentProcessId();
  request.crashpad_info_address =
      reinterpret_cast<WinVMAddress>(&crashpad_info);

  server_thread().Start();
  ScopedStopServerAndJoinThread scoped_stop_server_and_join_thread(
      &server(), &server_thread());

  ASSERT_NO_FATAL_FAILURE(delegate().WaitForStart());

  ASSERT_TRUE(SendRequest(Connect(), &request, sizeof(request), &response));

  ASSERT_EQ(1, delegate().registered_processes().size());
  VerifyRegistration(*delegate().registered_processes()[0], request, response);
}

TEST_F(RegistrationServerTest, ForgedClientId) {
  // Skip this test on pre-Vista as the forged PID detection is not supported
  // there.
  OSVERSIONINFO vi = {0};
  vi.dwOSVersionInfoSize = sizeof(vi);
  GetVersionEx(&vi);
  if (vi.dwMajorVersion < 6)
    return;

  RegistrationRequest request = {0};
  RegistrationResponse response = {0};
  CrashpadInfo crashpad_info;
  // Note that we forge the PID here.
  request.client_process_id = GetCurrentProcessId() + 1;
  request.crashpad_info_address =
      reinterpret_cast<WinVMAddress>(&crashpad_info);

  server_thread().Start();
  ScopedStopServerAndJoinThread scoped_stop_server_and_join_thread(
      &server(), &server_thread());

  ASSERT_NO_FATAL_FAILURE(delegate().WaitForStart());

  ASSERT_FALSE(SendRequest(Connect(), &request, sizeof(request), &response));
  ASSERT_EQ(0, delegate().registered_processes().size());

  // Correct the PID and verify that this was the only reason we failed.
  request.client_process_id = GetCurrentProcessId();
  ASSERT_TRUE(SendRequest(Connect(), &request, sizeof(request), &response));
  ASSERT_EQ(1, delegate().registered_processes().size());
  VerifyRegistration(*delegate().registered_processes()[0], request, response);
}

TEST_F(RegistrationServerTest, RegisterClientFails) {
  RegistrationRequest request = {0};
  RegistrationResponse response = {0};
  CrashpadInfo crashpad_info;
  request.client_process_id = GetCurrentProcessId();
  request.crashpad_info_address =
      reinterpret_cast<WinVMAddress>(&crashpad_info);

  server_thread().Start();
  ScopedStopServerAndJoinThread scoped_stop_server_and_join_thread(
      &server(), &server_thread());

  ASSERT_NO_FATAL_FAILURE(delegate().WaitForStart());

  // Simulate some failures
  delegate().set_fail_mode(true);
  for (int i = 0; i < 10; ++i) {
    ASSERT_FALSE(SendRequest(Connect(), &request, sizeof(request), &response));
    ASSERT_EQ(0, delegate().registered_processes().size());
  }

  // Now verify that a valid response may still be processed.
  delegate().set_fail_mode(false);
  ASSERT_TRUE(SendRequest(Connect(), &request, sizeof(request), &response));

  ASSERT_EQ(1, delegate().registered_processes().size());
  VerifyRegistration(*delegate().registered_processes()[0], request, response);
}

TEST_F(RegistrationServerTest, BadRequests) {
  server_thread().Start();
  ScopedStopServerAndJoinThread scoped_stop_server_and_join_thread(
      &server(), &server_thread());
  ASSERT_NO_FATAL_FAILURE(delegate().WaitForStart());

  RegistrationRequest request = {0};
  RegistrationResponse response = {0};
  CrashpadInfo crashpad_info;
  request.client_process_id = GetCurrentProcessId();
  request.crashpad_info_address =
      reinterpret_cast<WinVMAddress>(&crashpad_info);

  // Concatenate a valid request with a single byte of garbage.
  std::vector<char> extra_long;
  extra_long.insert(extra_long.begin(),
                    reinterpret_cast<char*>(&request),
                    reinterpret_cast<char*>(&request) + sizeof(request));
  extra_long.push_back('x');

  for (int i = 0; i < 10; ++i) {
    ASSERT_FALSE(SendRequest(Connect(), "a", 1, &response));
    ASSERT_FALSE(SendRequest(
        Connect(), extra_long.data(), extra_long.size(), &response));
    ASSERT_TRUE(Connect().is_valid());
  }

  // Now verify that a valid response may still be processed.

  ASSERT_TRUE(SendRequest(Connect(), &request, sizeof(request), &response));

  ASSERT_EQ(1, delegate().registered_processes().size());
  VerifyRegistration(*delegate().registered_processes()[0], request, response);
}

TEST_F(RegistrationServerTest, OverlappingRequests) {
  server_thread().Start();
  ScopedStopServerAndJoinThread scoped_stop_server_and_join_thread(
      &server(), &server_thread());
  ASSERT_NO_FATAL_FAILURE(delegate().WaitForStart());

  RegistrationRequest request = {0};
  RegistrationResponse response_1 = {0};
  RegistrationResponse response_2 = {0};
  RegistrationResponse response_3 = {0};
  CrashpadInfo crashpad_info;
  request.client_process_id = GetCurrentProcessId();
  request.crashpad_info_address =
      reinterpret_cast<WinVMAddress>(&crashpad_info);

  ScopedFileHANDLE connection_1 = Connect();
  ASSERT_TRUE(connection_1.is_valid());
  ScopedFileHANDLE connection_2 = Connect();
  ASSERT_TRUE(connection_2.is_valid());
  ScopedFileHANDLE connection_3 = Connect();
  ASSERT_TRUE(connection_3.is_valid());

  ASSERT_FALSE(SendRequest(connection_1.Pass(), "a", 1, &response_1));

  ASSERT_TRUE(
      SendRequest(connection_2.Pass(), &request, sizeof(request), &response_2));

  ASSERT_TRUE(Connect().is_valid());

  ASSERT_TRUE(
      SendRequest(connection_3.Pass(), &request, sizeof(request), &response_3));

  ASSERT_EQ(2, delegate().registered_processes().size());
  VerifyRegistration(
      *delegate().registered_processes()[0], request, response_2);
  VerifyRegistration(
      *delegate().registered_processes()[1], request, response_3);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
