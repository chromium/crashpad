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

#include "handler/win/registration_pipe_state.h"

#include <windows.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "client/crashpad_info.h"
#include "client/registration_protocol_win.h"
#include "gtest/gtest.h"
#include "handler/win/registration_test_base.h"
#include "util/win/scoped_handle.h"

namespace crashpad {
namespace test {
namespace {

class RegistrationRegistrationPipeStateTest : public RegistrationTestBase {
 public:
  RegistrationRegistrationPipeStateTest() : pipe_state_() {}

  void SetUp() override {
    ScopedFileHANDLE pipe(
        CreateNamedPipe(pipe_name().c_str(),
                        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED |
                            FILE_FLAG_FIRST_PIPE_INSTANCE,
                        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                        1,
                        512,  // nOutBufferSize
                        512,  // nInBufferSize
                        20,  // nDefaultTimeOut
                        nullptr));  // lpSecurityAttributes
    ASSERT_TRUE(pipe.is_valid());
    pipe_state_.reset(new RegistrationPipeState(pipe.Pass(), &delegate()));
  }

  ~RegistrationRegistrationPipeStateTest() override {}

  RegistrationPipeState& pipe_state() {
    DCHECK(pipe_state_.get());
    return *pipe_state_;
  }

 private:
  scoped_ptr<RegistrationPipeState> pipe_state_;
  DISALLOW_COPY_AND_ASSIGN(RegistrationRegistrationPipeStateTest);
};

TEST_F(RegistrationRegistrationPipeStateTest, CancelIoWhenConnectIsComplete) {
  //  -> Connecting
  ASSERT_TRUE(pipe_state().Initialize());

  ScopedFileHANDLE client(Connect());

  ASSERT_TRUE(client.is_valid());

  // Connect completion.
  ASSERT_EQ(WAIT_OBJECT_0,
            WaitForSingleObject(pipe_state().completion_event(), INFINITE));

  // Connecting -> Stopping
  pipe_state().Stop();

  // Stop completion.
  ASSERT_EQ(WAIT_OBJECT_0,
            WaitForSingleObject(pipe_state().completion_event(), INFINITE));
}

TEST_F(RegistrationRegistrationPipeStateTest, CancelIoWhenReadIsComplete) {
  // -> Connecting
  ASSERT_TRUE(pipe_state().Initialize());

  ScopedFileHANDLE client(Connect());

  ASSERT_TRUE(client.is_valid());

  // Connect completion.
  ASSERT_EQ(WAIT_OBJECT_0,
            WaitForSingleObject(pipe_state().completion_event(), INFINITE));

  // Connecting -> Reading
  ASSERT_TRUE(pipe_state().OnCompletion());

  RegistrationRequest request = {0};
  CrashpadInfo crashpad_info;
  request.client_process_id = GetCurrentProcessId();
  request.crashpad_info_address =
      reinterpret_cast<WinVMAddress>(&crashpad_info);

  ASSERT_TRUE(WriteRequest(client.get(), &request, sizeof(request)));

  // Read completion.
  ASSERT_EQ(WAIT_OBJECT_0,
            WaitForSingleObject(pipe_state().completion_event(), INFINITE));

  // Reading -> Stopping
  pipe_state().Stop();

  // Stop completion.
  ASSERT_EQ(WAIT_OBJECT_0,
            WaitForSingleObject(pipe_state().completion_event(), INFINITE));
}

TEST_F(RegistrationRegistrationPipeStateTest, CancelIoWhenWriteIsComplete) {
  // -> Connecting
  ASSERT_TRUE(pipe_state().Initialize());

  ScopedFileHANDLE client(Connect());

  ASSERT_TRUE(client.is_valid());

  // Connect completion.
  ASSERT_EQ(WAIT_OBJECT_0,
            WaitForSingleObject(pipe_state().completion_event(), INFINITE));

  // Connecting -> Reading
  ASSERT_TRUE(pipe_state().OnCompletion());

  RegistrationRequest request = {0};
  CrashpadInfo crashpad_info;
  request.client_process_id = GetCurrentProcessId();
  request.crashpad_info_address =
      reinterpret_cast<WinVMAddress>(&crashpad_info);

  ASSERT_TRUE(WriteRequest(client.get(), &request, sizeof(request)));

  // Read completion.
  ASSERT_EQ(WAIT_OBJECT_0,
            WaitForSingleObject(pipe_state().completion_event(), INFINITE));

  // Reading -> Writing -> Waiting for Close
  ASSERT_TRUE(pipe_state().OnCompletion());

  RegistrationResponse response = {0};
  ASSERT_TRUE(ReadResponse(client.get(), &response));

  // Waiting for Close -> Stopping
  pipe_state().Stop();

  // Stop completion.
  ASSERT_EQ(WAIT_OBJECT_0,
            WaitForSingleObject(pipe_state().completion_event(), INFINITE));
}

TEST_F(RegistrationRegistrationPipeStateTest, CancelIoWhenCloseIsComplete) {
  // -> Connecting
  ASSERT_TRUE(pipe_state().Initialize());

  ScopedFileHANDLE client(Connect());

  ASSERT_TRUE(client.is_valid());

  // Connect completion.
  ASSERT_EQ(WAIT_OBJECT_0,
            WaitForSingleObject(pipe_state().completion_event(), INFINITE));

  // Connecting -> Reading
  ASSERT_TRUE(pipe_state().OnCompletion());

  RegistrationRequest request = {0};
  CrashpadInfo crashpad_info;
  request.client_process_id = GetCurrentProcessId();
  request.crashpad_info_address =
      reinterpret_cast<WinVMAddress>(&crashpad_info);

  ASSERT_TRUE(WriteRequest(client.get(), &request, sizeof(request)));

  // Read completion.
  ASSERT_EQ(WAIT_OBJECT_0,
            WaitForSingleObject(pipe_state().completion_event(), INFINITE));

  // Reading -> Writing -> Waiting for Close
  ASSERT_TRUE(pipe_state().OnCompletion());

  RegistrationResponse response = {0};
  ASSERT_TRUE(ReadResponse(client.get(), &response));

  client.reset();

  // Wait for close completion.
  ASSERT_EQ(WAIT_OBJECT_0,
            WaitForSingleObject(pipe_state().completion_event(), INFINITE));

  // Waiting for Close -> Stopping
  pipe_state().Stop();

  // Stop completion.
  ASSERT_EQ(WAIT_OBJECT_0,
            WaitForSingleObject(pipe_state().completion_event(), INFINITE));
}

TEST_F(RegistrationRegistrationPipeStateTest, FullCycle) {
  // -> Connecting
  ASSERT_TRUE(pipe_state().Initialize());

  ScopedFileHANDLE client(Connect());

  ASSERT_TRUE(client.is_valid());

  // Connect completion.
  ASSERT_EQ(WAIT_OBJECT_0,
            WaitForSingleObject(pipe_state().completion_event(), INFINITE));

  // Connecting -> Reading
  ASSERT_TRUE(pipe_state().OnCompletion());

  RegistrationRequest request = {0};
  CrashpadInfo crashpad_info;
  request.client_process_id = GetCurrentProcessId();
  request.crashpad_info_address =
      reinterpret_cast<WinVMAddress>(&crashpad_info);

  ASSERT_TRUE(WriteRequest(client.get(), &request, sizeof(request)));

  // Read completion.
  ASSERT_EQ(WAIT_OBJECT_0,
            WaitForSingleObject(pipe_state().completion_event(), INFINITE));

  // Reading -> Writing -> Waiting for Close
  ASSERT_TRUE(pipe_state().OnCompletion());

  RegistrationResponse response = {0};
  ASSERT_TRUE(ReadResponse(client.get(), &response));

  client.reset();

  // Wait for close completion.
  ASSERT_EQ(WAIT_OBJECT_0,
            WaitForSingleObject(pipe_state().completion_event(), INFINITE));

  // Waiting for Close -> Reset -> Connecting
  ASSERT_TRUE(pipe_state().OnCompletion());

  client = Connect();
  ASSERT_TRUE(client.is_valid());

  pipe_state().Stop();

  ASSERT_EQ(WAIT_OBJECT_0,
            WaitForSingleObject(pipe_state().completion_event(), INFINITE));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
