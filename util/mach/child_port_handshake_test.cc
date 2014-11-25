// Copyright 2014 The Crashpad Authors. All rights reserved.
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

#include "util/mach/child_port_handshake.h"

#include "base/mac/scoped_mach_port.h"
#include "gtest/gtest.h"
#include "util/mach/child_port_types.h"
#include "util/mach/mach_extensions.h"
#include "util/test/multiprocess.h"

namespace crashpad {
namespace test {
namespace {

class ChildPortHandshakeTest : public Multiprocess {
 public:
  enum TestType {
    kTestTypeChildChecksIn = 0,
    kTestTypeChildDoesNotCheckIn_ReadsPipe,
    kTestTypeChildDoesNotCheckIn,
    kTestTypeTokenIncorrect,
    kTestTypeTokenIncorrectThenCorrect,
  };

  explicit ChildPortHandshakeTest(TestType test_type)
      : Multiprocess(), child_port_handshake_(), test_type_(test_type) {}
  ~ChildPortHandshakeTest() {}

 private:
  // Multiprocess:

  void MultiprocessParent() override {
    base::mac::ScopedMachSendRight child_port(
        child_port_handshake_.RunServer());
    switch (test_type_) {
      case kTestTypeChildChecksIn:
      case kTestTypeTokenIncorrectThenCorrect:
        EXPECT_EQ(bootstrap_port, child_port);
        break;

      case kTestTypeChildDoesNotCheckIn_ReadsPipe:
      case kTestTypeChildDoesNotCheckIn:
      case kTestTypeTokenIncorrect:
        EXPECT_EQ(kMachPortNull, child_port);
        break;
    }
  }

  void MultiprocessChild() override {
    int read_pipe = child_port_handshake_.ReadPipeFD();
    switch (test_type_) {
      case kTestTypeChildChecksIn:
        ChildPortHandshake::RunClient(
            read_pipe, bootstrap_port, MACH_MSG_TYPE_COPY_SEND);
        break;

      case kTestTypeChildDoesNotCheckIn_ReadsPipe: {
        // Don’t run the standard client routine. Instead, drain the pipe, which
        // will get the parent to the point that it begins waiting for a
        // check-in message. Then, exit. The pipe is drained using the same
        // implementation that the real client would use.
        child_port_token_t token;
        std::string service_name;
        ChildPortHandshake::RunClientInternal_ReadPipe(
            read_pipe, &token, &service_name);
        break;
      }

      case kTestTypeChildDoesNotCheckIn:
        break;

      case kTestTypeTokenIncorrect: {
        // Don’t run the standard client routine. Instead, read the token and
        // service name, mutate the token, and then check in with the bad token.
        // The parent should reject the message.
        child_port_token_t token;
        std::string service_name;
        ChildPortHandshake::RunClientInternal_ReadPipe(
            read_pipe, &token, &service_name);
        child_port_token_t bad_token = ~token;
        ChildPortHandshake::RunClientInternal_SendCheckIn(
            service_name, bad_token, mach_task_self(), MACH_MSG_TYPE_COPY_SEND);
        break;
      }

      case kTestTypeTokenIncorrectThenCorrect: {
        // Don’t run the standard client routine. Instead, read the token and
        // service name. Mutate the token, and check in with the bad token,
        // expecting the parent to reject the message. Then, check in with the
        // correct token, expecting the parent to accept it.
        child_port_token_t token;
        std::string service_name;
        ChildPortHandshake::RunClientInternal_ReadPipe(
            read_pipe, &token, &service_name);
        child_port_token_t bad_token = ~token;
        ChildPortHandshake::RunClientInternal_SendCheckIn(
            service_name, bad_token, mach_task_self(), MACH_MSG_TYPE_COPY_SEND);
        ChildPortHandshake::RunClientInternal_SendCheckIn(
            service_name, token, bootstrap_port, MACH_MSG_TYPE_COPY_SEND);
        break;
      }
    }
  }

 private:
  ChildPortHandshake child_port_handshake_;
  TestType test_type_;

  DISALLOW_COPY_AND_ASSIGN(ChildPortHandshakeTest);
};

TEST(ChildPortHandshake, ChildChecksIn) {
  // In this test, the client checks in with the server normally. It sends a
  // copy of its bootstrap port to the server, because both parent and child
  // should have the same bootstrap port, allowing for verification.
  ChildPortHandshakeTest test(ChildPortHandshakeTest::kTestTypeChildChecksIn);
  test.Run();
}

TEST(ChildPortHandshake, ChildDoesNotCheckIn) {
  // In this test, the client exits without checking in. This tests that the
  // server properly detects that it has lost a client. Whether or not the
  // client closes the pipe before the server writes to it is a race, and the
  // server needs to be able to detect client loss in both cases, so the
  // ChildDoesNotCheckIn_ReadsPipe and NoChild tests also exist to test these
  // individual cases more deterministically.
  ChildPortHandshakeTest test(
      ChildPortHandshakeTest::kTestTypeChildDoesNotCheckIn);
  test.Run();
}

TEST(ChildPortHandshake, ChildDoesNotCheckIn_ReadsPipe) {
  // In this test, the client reads from its pipe, and subsequently exits
  // without checking in. This tests that the server properly detects that it
  // has lost its client after sending instructions to it via the pipe, while
  // waiting for a check-in message.
  ChildPortHandshakeTest test(
      ChildPortHandshakeTest::kTestTypeChildDoesNotCheckIn_ReadsPipe);
  test.Run();
}

TEST(ChildPortHandshake, TokenIncorrect) {
  // In this test, the client checks in with the server with an incorrect token
  // value and a copy of its own task port. The server should reject the message
  // because of the invalid token, and return MACH_PORT_NULL to its caller.
  ChildPortHandshakeTest test(ChildPortHandshakeTest::kTestTypeTokenIncorrect);
  test.Run();
}

TEST(ChildPortHandshake, TokenIncorrectThenCorrect) {
  // In this test, the client checks in with the server with an incorrect token
  // value and a copy of its own task port, and subsequently, the correct token
  // value and a copy of its bootstrap port. The server should reject the first
  // because of the invalid token, but it should continue waiting for a message
  // with a valid token as long as the pipe remains open. It should wind wind up
  // returning the bootstrap port, allowing for verification.
  ChildPortHandshakeTest test(
      ChildPortHandshakeTest::kTestTypeTokenIncorrectThenCorrect);
  test.Run();
}

TEST(ChildPortHandshake, NoChild) {
  // In this test, the client never checks in with the parent because the child
  // never even runs. This tests that the server properly detects that it has
  // no client at all, and does not terminate execution with an error such as
  // “broken pipe” when attempting to send instructions to the client. This test
  // is similar to ChildDoesNotCheckIn, but because there’s no child at all, the
  // server is guaranteed to see that its pipe partner is gone.
  ChildPortHandshake child_port_handshake;
  base::mac::ScopedMachSendRight child_port(child_port_handshake.RunServer());
  EXPECT_EQ(kMachPortNull, child_port);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
