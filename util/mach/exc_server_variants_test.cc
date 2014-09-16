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

#include "util/mach/exc_server_variants.h"

#include <mach/mach.h>
#include <string.h>

#include "base/basictypes.h"
#include "base/strings/stringprintf.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "util/mach/exception_behaviors.h"
#include "util/mach/mach_extensions.h"
#include "util/test/mac/mach_errors.h"
#include "util/test/mac/mach_multiprocess.h"

namespace {

using namespace crashpad;
using namespace crashpad::test;
using namespace testing;

// Fake Mach ports. These aren’t used as ports in these tests, they’re just used
// as cookies to make sure that the correct values get passed to the correct
// places.
const mach_port_t kClientRemotePort = 0x01010101;
const mach_port_t kServerLocalPort = 0x02020202;
const mach_port_t kExceptionThreadPort = 0x03030303;
const mach_port_t kExceptionTaskPort = 0x04040404;

// Other fake exception values.
const exception_type_t kExceptionType = EXC_BAD_ACCESS;

// Test using an exception code with the high bit set to ensure that it gets
// promoted to the wider mach_exception_data_type_t type as a signed quantity.
const exception_data_type_t kTestExceptonCodes[] = {
    KERN_PROTECTION_FAILURE,
    static_cast<exception_data_type_t>(0xfedcba98),
};

const exception_data_type_t kTestMachExceptionCodes[] = {
    KERN_PROTECTION_FAILURE,
    static_cast<exception_data_type_t>(0xfedcba9876543210),
};

const thread_state_flavor_t kThreadStateFlavor = MACHINE_THREAD_STATE;
const mach_msg_type_number_t kThreadStateFlavorCount =
    MACHINE_THREAD_STATE_COUNT;

void InitializeMachMsgPortDescriptor(mach_msg_port_descriptor_t* descriptor,
                                     mach_port_t port) {
  descriptor->name = port;
  descriptor->disposition = MACH_MSG_TYPE_MOVE_SEND;
  descriptor->type = MACH_MSG_PORT_DESCRIPTOR;
}

// The definitions of the request and reply structures from mach_exc.h aren’t
// available here. They need custom initialization code, and the reply
// structures need verification code too, so duplicate the expected definitions
// of the structures from both exc.h and mach_exc.h here in this file, and
// provide the initialization and verification code as methods in true
// object-oriented fashion.

struct __attribute__((packed, aligned(4))) ExceptionRaiseRequest {
  mach_msg_header_t Head;
  mach_msg_body_t msgh_body;
  mach_msg_port_descriptor_t thread;
  mach_msg_port_descriptor_t task;
  NDR_record_t NDR;
  exception_type_t exception;
  mach_msg_type_number_t codeCnt;
  integer_t code[2];

  void InitializeForTesting() {
    memset(this, 0xa5, sizeof(*this));
    Head.msgh_bits =
        MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE) |
        MACH_MSGH_BITS_COMPLEX;
    Head.msgh_size = sizeof(*this);
    Head.msgh_remote_port = kClientRemotePort;
    Head.msgh_local_port = kServerLocalPort;
    Head.msgh_id = 2401;
    msgh_body.msgh_descriptor_count = 2;
    InitializeMachMsgPortDescriptor(&thread, kExceptionThreadPort);
    InitializeMachMsgPortDescriptor(&task, kExceptionTaskPort);
    NDR = NDR_record;
    exception = kExceptionType;
    codeCnt = 2;
    code[0] = kTestExceptonCodes[0];
    code[1] = kTestExceptonCodes[1];
  }
};

struct __attribute__((packed, aligned(4))) ExceptionRaiseReply {
  mach_msg_header_t Head;
  NDR_record_t NDR;
  kern_return_t RetCode;

  void InitializeForTesting() {
    memset(this, 0x5a, sizeof(*this));
    RetCode = KERN_FAILURE;
  }

  // Verify accepts a |behavior| parameter because the same message format and
  // verification function is used for ExceptionRaiseReply and
  // MachExceptionRaiseReply. Knowing which behavior is expected allows the
  // message ID to be checked.
  void Verify(exception_behavior_t behavior) {
    EXPECT_EQ(static_cast<mach_msg_bits_t>(
                  MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0)),
              Head.msgh_bits);
    EXPECT_EQ(sizeof(*this), Head.msgh_size);
    EXPECT_EQ(kClientRemotePort, Head.msgh_remote_port);
    EXPECT_EQ(kMachPortNull, Head.msgh_local_port);
    switch (behavior) {
      case EXCEPTION_DEFAULT:
        EXPECT_EQ(2501, Head.msgh_id);
        break;
      case EXCEPTION_DEFAULT | kMachExceptionCodes:
        EXPECT_EQ(2505, Head.msgh_id);
        break;
      default:
        ADD_FAILURE() << "behavior " << behavior << ", Head.msgh_id "
                      << Head.msgh_id;
        break;
    }
    EXPECT_EQ(0, memcmp(&NDR, &NDR_record, sizeof(NDR)));
    EXPECT_EQ(KERN_SUCCESS, RetCode);
  }
};

struct __attribute__((packed, aligned(4))) ExceptionRaiseStateRequest {
  mach_msg_header_t Head;
  NDR_record_t NDR;
  exception_type_t exception;
  mach_msg_type_number_t codeCnt;
  integer_t code[2];
  int flavor;
  mach_msg_type_number_t old_stateCnt;
  natural_t old_state[THREAD_STATE_MAX];

  void InitializeForTesting() {
    memset(this, 0xa5, sizeof(*this));
    Head.msgh_bits =
        MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE);
    Head.msgh_size = sizeof(*this);
    Head.msgh_remote_port = kClientRemotePort;
    Head.msgh_local_port = kServerLocalPort;
    Head.msgh_id = 2402;
    NDR = NDR_record;
    exception = kExceptionType;
    codeCnt = 2;
    code[0] = kTestExceptonCodes[0];
    code[1] = kTestExceptonCodes[1];
    flavor = kThreadStateFlavor;
    old_stateCnt = kThreadStateFlavorCount;

    // Adjust the message size for the data that it’s actually carrying, which
    // may be smaller than the maximum that it can carry.
    Head.msgh_size += sizeof(old_state[0]) * old_stateCnt - sizeof(old_state);
  }
};

struct __attribute__((packed, aligned(4))) ExceptionRaiseStateReply {
  mach_msg_header_t Head;
  NDR_record_t NDR;
  kern_return_t RetCode;
  int flavor;
  mach_msg_type_number_t new_stateCnt;
  natural_t new_state[THREAD_STATE_MAX];

  void InitializeForTesting() {
    memset(this, 0x5a, sizeof(*this));
    RetCode = KERN_FAILURE;
  }

  // Verify accepts a |behavior| parameter because the same message format and
  // verification function is used for ExceptionRaiseStateReply,
  // ExceptionRaiseStateIdentityReply, MachExceptionRaiseStateReply, and
  // MachExceptionRaiseStateIdentityReply. Knowing which behavior is expected
  // allows the message ID to be checked.
  void Verify(exception_behavior_t behavior) {
    EXPECT_EQ(static_cast<mach_msg_bits_t>(
                  MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0)),
              Head.msgh_bits);
    EXPECT_EQ(sizeof(*this), Head.msgh_size);
    EXPECT_EQ(kClientRemotePort, Head.msgh_remote_port);
    EXPECT_EQ(kMachPortNull, Head.msgh_local_port);
    switch (behavior) {
      case EXCEPTION_STATE:
        EXPECT_EQ(2502, Head.msgh_id);
        break;
      case EXCEPTION_STATE_IDENTITY:
        EXPECT_EQ(2503, Head.msgh_id);
        break;
      case EXCEPTION_STATE | kMachExceptionCodes:
        EXPECT_EQ(2506, Head.msgh_id);
        break;
      case EXCEPTION_STATE_IDENTITY | kMachExceptionCodes:
        EXPECT_EQ(2507, Head.msgh_id);
        break;
      default:
        ADD_FAILURE() << "behavior " << behavior << ", Head.msgh_id "
                      << Head.msgh_id;
        break;
    }
    EXPECT_EQ(0, memcmp(&NDR, &NDR_record, sizeof(NDR)));
    EXPECT_EQ(KERN_SUCCESS, RetCode);
    EXPECT_EQ(kThreadStateFlavor, flavor);
    EXPECT_EQ(arraysize(new_state), new_stateCnt);
  }
};

struct __attribute__((packed, aligned(4))) ExceptionRaiseStateIdentityRequest {
  mach_msg_header_t Head;
  mach_msg_body_t msgh_body;
  mach_msg_port_descriptor_t thread;
  mach_msg_port_descriptor_t task;
  NDR_record_t NDR;
  exception_type_t exception;
  mach_msg_type_number_t codeCnt;
  integer_t code[2];
  int flavor;
  mach_msg_type_number_t old_stateCnt;
  natural_t old_state[THREAD_STATE_MAX];

  void InitializeForTesting() {
    memset(this, 0xa5, sizeof(*this));
    Head.msgh_bits =
        MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE) |
        MACH_MSGH_BITS_COMPLEX;
    Head.msgh_size = sizeof(*this);
    Head.msgh_remote_port = kClientRemotePort;
    Head.msgh_local_port = kServerLocalPort;
    Head.msgh_id = 2403;
    msgh_body.msgh_descriptor_count = 2;
    InitializeMachMsgPortDescriptor(&thread, kExceptionThreadPort);
    InitializeMachMsgPortDescriptor(&task, kExceptionTaskPort);
    NDR = NDR_record;
    exception = kExceptionType;
    codeCnt = 2;
    code[0] = kTestExceptonCodes[0];
    code[1] = kTestExceptonCodes[1];
    flavor = kThreadStateFlavor;
    old_stateCnt = kThreadStateFlavorCount;

    // Adjust the message size for the data that it’s actually carrying, which
    // may be smaller than the maximum that it can carry.
    Head.msgh_size += sizeof(old_state[0]) * old_stateCnt - sizeof(old_state);
  }
};

// The reply messages for exception_raise_state and
// exception_raise_state_identity are identical.
typedef ExceptionRaiseStateReply ExceptionRaiseStateIdentityReply;

struct __attribute__((packed, aligned(4))) MachExceptionRaiseRequest {
  mach_msg_header_t Head;
  mach_msg_body_t msgh_body;
  mach_msg_port_descriptor_t thread;
  mach_msg_port_descriptor_t task;
  NDR_record_t NDR;
  exception_type_t exception;
  mach_msg_type_number_t codeCnt;
  int64_t code[2];

  void InitializeForTesting() {
    memset(this, 0xa5, sizeof(*this));
    Head.msgh_bits =
        MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE) |
        MACH_MSGH_BITS_COMPLEX;
    Head.msgh_size = sizeof(*this);
    Head.msgh_remote_port = kClientRemotePort;
    Head.msgh_local_port = kServerLocalPort;
    Head.msgh_id = 2405;
    msgh_body.msgh_descriptor_count = 2;
    InitializeMachMsgPortDescriptor(&thread, kExceptionThreadPort);
    InitializeMachMsgPortDescriptor(&task, kExceptionTaskPort);
    NDR = NDR_record;
    exception = kExceptionType;
    codeCnt = 2;
    code[0] = kTestMachExceptionCodes[0];
    code[1] = kTestMachExceptionCodes[1];
  }
};

// The reply messages for exception_raise and mach_exception_raise are
// identical.
typedef ExceptionRaiseReply MachExceptionRaiseReply;

struct __attribute__((packed, aligned(4))) MachExceptionRaiseStateRequest {
  mach_msg_header_t Head;
  NDR_record_t NDR;
  exception_type_t exception;
  mach_msg_type_number_t codeCnt;
  int64_t code[2];
  int flavor;
  mach_msg_type_number_t old_stateCnt;
  natural_t old_state[THREAD_STATE_MAX];

  void InitializeForTesting() {
    memset(this, 0xa5, sizeof(*this));
    Head.msgh_bits =
        MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE);
    Head.msgh_size = sizeof(*this);
    Head.msgh_remote_port = kClientRemotePort;
    Head.msgh_local_port = kServerLocalPort;
    Head.msgh_id = 2406;
    NDR = NDR_record;
    exception = kExceptionType;
    codeCnt = 2;
    code[0] = kTestMachExceptionCodes[0];
    code[1] = kTestMachExceptionCodes[1];
    flavor = kThreadStateFlavor;
    old_stateCnt = kThreadStateFlavorCount;

    // Adjust the message size for the data that it’s actually carrying, which
    // may be smaller than the maximum that it can carry.
    Head.msgh_size += sizeof(old_state[0]) * old_stateCnt - sizeof(old_state);
  }
};

// The reply messages for exception_raise_state and mach_exception_raise_state
// are identical.
typedef ExceptionRaiseStateReply MachExceptionRaiseStateReply;

struct __attribute__((packed,
                      aligned(4))) MachExceptionRaiseStateIdentityRequest {
  mach_msg_header_t Head;
  mach_msg_body_t msgh_body;
  mach_msg_port_descriptor_t thread;
  mach_msg_port_descriptor_t task;
  NDR_record_t NDR;
  exception_type_t exception;
  mach_msg_type_number_t codeCnt;
  int64_t code[2];
  int flavor;
  mach_msg_type_number_t old_stateCnt;
  natural_t old_state[THREAD_STATE_MAX];

  void InitializeForTesting() {
    memset(this, 0xa5, sizeof(*this));
    Head.msgh_bits =
        MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE) |
        MACH_MSGH_BITS_COMPLEX;
    Head.msgh_size = sizeof(*this);
    Head.msgh_remote_port = kClientRemotePort;
    Head.msgh_local_port = kServerLocalPort;
    Head.msgh_id = 2407;
    msgh_body.msgh_descriptor_count = 2;
    InitializeMachMsgPortDescriptor(&thread, kExceptionThreadPort);
    InitializeMachMsgPortDescriptor(&task, kExceptionTaskPort);
    NDR = NDR_record;
    exception = kExceptionType;
    codeCnt = 2;
    code[0] = kTestMachExceptionCodes[0];
    code[1] = kTestMachExceptionCodes[1];
    flavor = kThreadStateFlavor;
    old_stateCnt = kThreadStateFlavorCount;

    // Adjust the message size for the data that it’s actually carrying, which
    // may be smaller than the maximum that it can carry.
    Head.msgh_size += sizeof(old_state[0]) * old_stateCnt - sizeof(old_state);
  }
};

// The reply messages for exception_raise_state_identity and
// mach_exception_raise_state_identity are identical.
typedef ExceptionRaiseStateIdentityReply MachExceptionRaiseStateIdentityReply;

// InvalidRequest and BadIDErrorReply are used to test that
// UniversalMachExcServer deals appropriately with messages that it does not
// understand: messages with an unknown Head.msgh_id.

struct __attribute__((packed, aligned(4))) InvalidRequest
    : public mach_msg_empty_send_t {
  void InitializeForTesting(mach_msg_id_t id) {
    memset(this, 0xa5, sizeof(*this));
    header.msgh_bits =
        MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE);
    header.msgh_size = sizeof(*this);
    header.msgh_remote_port = kClientRemotePort;
    header.msgh_local_port = kServerLocalPort;
    header.msgh_id = id;
  }
};

struct __attribute__((packed, aligned(4))) BadIDErrorReply
    : public mig_reply_error_t {
  void InitializeForTesting() {
    memset(this, 0x5a, sizeof(*this));
    RetCode = KERN_FAILURE;
  }

  void Verify(mach_msg_id_t id) {
    EXPECT_EQ(static_cast<mach_msg_bits_t>(
                  MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0)),
              Head.msgh_bits);
    EXPECT_EQ(sizeof(*this), Head.msgh_size);
    EXPECT_EQ(kClientRemotePort, Head.msgh_remote_port);
    EXPECT_EQ(kMachPortNull, Head.msgh_local_port);
    EXPECT_EQ(id + 100, Head.msgh_id);
    EXPECT_EQ(0, memcmp(&NDR, &NDR_record, sizeof(NDR)));
    EXPECT_EQ(MIG_BAD_ID, RetCode);
  }
};

class MockUniversalMachExcServer : public UniversalMachExcServer {
 public:
  struct ConstExceptionCodes {
    const mach_exception_data_type_t* code;
    mach_msg_type_number_t code_count;
  };
  struct ThreadState {
    thread_state_t state;
    mach_msg_type_number_t* state_count;
  };
  struct ConstThreadState {
    const natural_t* state;
    mach_msg_type_number_t* state_count;
  };

  // CatchMachException is the method to mock, but it has 13 parameters, and
  // gmock can only mock methods with up to 10 parameters. Coalesce some related
  // parameters together into structs, and call a mocked method.
  virtual kern_return_t CatchMachException(
      exception_behavior_t behavior,
      exception_handler_t exception_port,
      thread_t thread,
      task_t task,
      exception_type_t exception,
      const mach_exception_data_type_t* code,
      mach_msg_type_number_t code_count,
      thread_state_flavor_t* flavor,
      const natural_t* old_state,
      mach_msg_type_number_t old_state_count,
      thread_state_t new_state,
      mach_msg_type_number_t* new_state_count,
      bool* destroy_complex_request) override {
    *destroy_complex_request = true;
    const ConstExceptionCodes exception_codes = {code, code_count};
    const ConstThreadState old_thread_state = {old_state, &old_state_count};
    ThreadState new_thread_state = {new_state, new_state_count};
    return MockCatchMachException(behavior,
                                  exception_port,
                                  thread,
                                  task,
                                  exception,
                                  &exception_codes,
                                  flavor,
                                  &old_thread_state,
                                  &new_thread_state);
  }

  MOCK_METHOD9(MockCatchMachException,
               kern_return_t(exception_behavior_t behavior,
                             exception_handler_t exception_port,
                             thread_t thread,
                             task_t task,
                             exception_type_t exception,
                             const ConstExceptionCodes* exception_codes,
                             thread_state_flavor_t* flavor,
                             const ConstThreadState* old_thread_state,
                             ThreadState* new_thread_state));
};

// Matcher for ConstExceptionCodes, testing that it carries 2 codes matching
// code_0 and code_1.
MATCHER_P2(AreExceptionCodes, code_0, code_1, "") {
  if (!arg) {
    return false;
  }

  if (arg->code_count == 2 && arg->code[0] == code_0 &&
      arg->code[1] == code_1) {
    return true;
  }

  *result_listener << "codes (";
  for (size_t index = 0; index < arg->code_count; ++index) {
    *result_listener << arg->code[index];
    if (index < arg->code_count - 1) {
      *result_listener << ", ";
    }
  }
  *result_listener << ")";

  return false;
}

// Matcher for ThreadState and ConstThreadState, testing that *state_count is
// present and matches the specified value. If 0 is specified for the count,
// state must be NULL (not present), otherwise state must be non-NULL (present).
MATCHER_P(IsThreadStateCount, state_count, "") {
  if (!arg) {
    return false;
  }
  if (!arg->state_count) {
    *result_listener << "state_count NULL";
    return false;
  }
  if (*(arg->state_count) != state_count) {
    *result_listener << "*state_count " << *(arg->state_count);
    return false;
  }
  if (state_count) {
    if (!arg->state) {
      *result_listener << "*state_count " << state_count << ", state NULL";
      return false;
    }
  } else {
    if (arg->state) {
      *result_listener << "*state_count 0, state non-NULL (" << arg->state
                       << ")";
      return false;
    }
  }
  return true;
}

template <typename T>
class ScopedDefaultValue {
 public:
  explicit ScopedDefaultValue(const T& default_value) {
    DefaultValue<T>::Set(default_value);
  }

  ~ScopedDefaultValue() { DefaultValue<T>::Clear(); }
};

TEST(ExcServerVariants, MockExceptionRaise) {
  ScopedDefaultValue<kern_return_t> default_kern_return_t(KERN_FAILURE);

  MockUniversalMachExcServer server;

  ExceptionRaiseRequest request;
  EXPECT_LE(sizeof(request), server.MachMessageServerRequestSize());
  request.InitializeForTesting();

  ExceptionRaiseReply reply;
  EXPECT_LE(sizeof(reply), server.MachMessageServerReplySize());
  reply.InitializeForTesting();

  const exception_behavior_t kExceptionBehavior = EXCEPTION_DEFAULT;

  EXPECT_CALL(server,
              MockCatchMachException(
                  kExceptionBehavior,
                  kServerLocalPort,
                  kExceptionThreadPort,
                  kExceptionTaskPort,
                  kExceptionType,
                  AreExceptionCodes(
                      kTestExceptonCodes[0], kTestExceptonCodes[1]),
                  Pointee(Eq(THREAD_STATE_NONE)),
                  IsThreadStateCount(0u),
                  IsThreadStateCount(0u)))
      .WillOnce(Return(KERN_SUCCESS))
      .RetiresOnSaturation();

  bool destroy_complex_request = false;
  EXPECT_TRUE(server.MachMessageServerFunction(
      reinterpret_cast<mach_msg_header_t*>(&request),
      reinterpret_cast<mach_msg_header_t*>(&reply),
      &destroy_complex_request));
  EXPECT_TRUE(destroy_complex_request);

  reply.Verify(kExceptionBehavior);
}

TEST(ExcServerVariants, MockExceptionRaiseState) {
  ScopedDefaultValue<kern_return_t> default_kern_return_t(KERN_FAILURE);

  MockUniversalMachExcServer server;

  ExceptionRaiseStateRequest request;
  EXPECT_LE(sizeof(request), server.MachMessageServerRequestSize());
  request.InitializeForTesting();

  ExceptionRaiseStateReply reply;
  EXPECT_LE(sizeof(reply), server.MachMessageServerReplySize());
  reply.InitializeForTesting();

  const exception_behavior_t kExceptionBehavior = EXCEPTION_STATE;

  EXPECT_CALL(server,
              MockCatchMachException(
                  kExceptionBehavior,
                  kServerLocalPort,
                  MACH_PORT_NULL,
                  MACH_PORT_NULL,
                  kExceptionType,
                  AreExceptionCodes(
                      kTestExceptonCodes[0], kTestExceptonCodes[1]),
                  Pointee(Eq(kThreadStateFlavor)),
                  IsThreadStateCount(kThreadStateFlavorCount),
                  IsThreadStateCount(arraysize(reply.new_state))))
      .WillOnce(Return(KERN_SUCCESS))
      .RetiresOnSaturation();

  bool destroy_complex_request = false;
  EXPECT_TRUE(server.MachMessageServerFunction(
      reinterpret_cast<mach_msg_header_t*>(&request),
      reinterpret_cast<mach_msg_header_t*>(&reply),
      &destroy_complex_request));

  // The request wasn’t complex, so nothing got a chance to change the value of
  // this variable.
  EXPECT_FALSE(destroy_complex_request);

  reply.Verify(kExceptionBehavior);
}

TEST(ExcServerVariants, MockExceptionRaiseStateIdentity) {
  ScopedDefaultValue<kern_return_t> default_kern_return_t(KERN_FAILURE);

  MockUniversalMachExcServer server;

  ExceptionRaiseStateIdentityRequest request;
  EXPECT_LE(sizeof(request), server.MachMessageServerRequestSize());
  request.InitializeForTesting();

  ExceptionRaiseStateIdentityReply reply;
  EXPECT_LE(sizeof(reply), server.MachMessageServerReplySize());
  reply.InitializeForTesting();

  const exception_behavior_t kExceptionBehavior = EXCEPTION_STATE_IDENTITY;

  EXPECT_CALL(server,
              MockCatchMachException(
                  kExceptionBehavior,
                  kServerLocalPort,
                  kExceptionThreadPort,
                  kExceptionTaskPort,
                  kExceptionType,
                  AreExceptionCodes(
                      kTestExceptonCodes[0], kTestExceptonCodes[1]),
                  Pointee(Eq(kThreadStateFlavor)),
                  IsThreadStateCount(kThreadStateFlavorCount),
                  IsThreadStateCount(arraysize(reply.new_state))))
      .WillOnce(Return(KERN_SUCCESS))
      .RetiresOnSaturation();

  bool destroy_complex_request = false;
  EXPECT_TRUE(server.MachMessageServerFunction(
      reinterpret_cast<mach_msg_header_t*>(&request),
      reinterpret_cast<mach_msg_header_t*>(&reply),
      &destroy_complex_request));
  EXPECT_TRUE(destroy_complex_request);

  reply.Verify(kExceptionBehavior);
}

TEST(ExcServerVariants, MockMachExceptionRaise) {
  ScopedDefaultValue<kern_return_t> default_kern_return_t(KERN_FAILURE);

  MockUniversalMachExcServer server;

  MachExceptionRaiseRequest request;
  EXPECT_LE(sizeof(request), server.MachMessageServerRequestSize());
  request.InitializeForTesting();

  MachExceptionRaiseReply reply;
  EXPECT_LE(sizeof(reply), server.MachMessageServerReplySize());
  reply.InitializeForTesting();

  const exception_behavior_t kExceptionBehavior =
      EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES;

  EXPECT_CALL(
      server,
      MockCatchMachException(
          kExceptionBehavior,
          kServerLocalPort,
          kExceptionThreadPort,
          kExceptionTaskPort,
          kExceptionType,
          AreExceptionCodes(
              kTestMachExceptionCodes[0], kTestMachExceptionCodes[1]),
          Pointee(Eq(THREAD_STATE_NONE)),
          IsThreadStateCount(0u),
          IsThreadStateCount(0u)))
      .WillOnce(Return(KERN_SUCCESS))
      .RetiresOnSaturation();

  bool destroy_complex_request = false;
  EXPECT_TRUE(server.MachMessageServerFunction(
      reinterpret_cast<mach_msg_header_t*>(&request),
      reinterpret_cast<mach_msg_header_t*>(&reply),
      &destroy_complex_request));
  EXPECT_TRUE(destroy_complex_request);

  reply.Verify(kExceptionBehavior);
}

TEST(ExcServerVariants, MockMachExceptionRaiseState) {
  ScopedDefaultValue<kern_return_t> default_kern_return_t(KERN_FAILURE);

  MockUniversalMachExcServer server;

  MachExceptionRaiseStateRequest request;
  EXPECT_LE(sizeof(request), server.MachMessageServerRequestSize());
  request.InitializeForTesting();

  MachExceptionRaiseStateReply reply;
  EXPECT_LE(sizeof(reply), server.MachMessageServerReplySize());
  reply.InitializeForTesting();

  const exception_behavior_t kExceptionBehavior =
      EXCEPTION_STATE | MACH_EXCEPTION_CODES;

  EXPECT_CALL(
      server,
      MockCatchMachException(
          kExceptionBehavior,
          kServerLocalPort,
          MACH_PORT_NULL,
          MACH_PORT_NULL,
          kExceptionType,
          AreExceptionCodes(
              kTestMachExceptionCodes[0], kTestMachExceptionCodes[1]),
          Pointee(Eq(kThreadStateFlavor)),
          IsThreadStateCount(kThreadStateFlavorCount),
          IsThreadStateCount(arraysize(reply.new_state))))
      .WillOnce(Return(KERN_SUCCESS))
      .RetiresOnSaturation();

  bool destroy_complex_request = false;
  EXPECT_TRUE(server.MachMessageServerFunction(
      reinterpret_cast<mach_msg_header_t*>(&request),
      reinterpret_cast<mach_msg_header_t*>(&reply),
      &destroy_complex_request));

  // The request wasn’t complex, so nothing got a chance to change the value of
  // this variable.
  EXPECT_FALSE(destroy_complex_request);

  reply.Verify(kExceptionBehavior);
}

TEST(ExcServerVariants, MockMachExceptionRaiseStateIdentity) {
  ScopedDefaultValue<kern_return_t> default_kern_return_t(KERN_FAILURE);

  MockUniversalMachExcServer server;

  MachExceptionRaiseStateIdentityRequest request;
  EXPECT_LE(sizeof(request), server.MachMessageServerRequestSize());
  request.InitializeForTesting();

  MachExceptionRaiseStateIdentityReply reply;
  EXPECT_LE(sizeof(reply), server.MachMessageServerReplySize());
  reply.InitializeForTesting();

  const exception_behavior_t kExceptionBehavior =
      EXCEPTION_STATE_IDENTITY | MACH_EXCEPTION_CODES;

  EXPECT_CALL(
      server,
      MockCatchMachException(
          kExceptionBehavior,
          kServerLocalPort,
          kExceptionThreadPort,
          kExceptionTaskPort,
          kExceptionType,
          AreExceptionCodes(
              kTestMachExceptionCodes[0], kTestMachExceptionCodes[1]),
          Pointee(Eq(kThreadStateFlavor)),
          IsThreadStateCount(kThreadStateFlavorCount),
          IsThreadStateCount(arraysize(reply.new_state))))
      .WillOnce(Return(KERN_SUCCESS))
      .RetiresOnSaturation();

  bool destroy_complex_request = false;
  EXPECT_TRUE(server.MachMessageServerFunction(
      reinterpret_cast<mach_msg_header_t*>(&request),
      reinterpret_cast<mach_msg_header_t*>(&reply),
      &destroy_complex_request));
  EXPECT_TRUE(destroy_complex_request);

  reply.Verify(kExceptionBehavior);
}

TEST(ExcServerVariants, MockUnknownID) {
  ScopedDefaultValue<kern_return_t> default_kern_return_t(KERN_FAILURE);

  MockUniversalMachExcServer server;

  // Make sure that a message with an unknown ID is handled appropriately.
  // UniversalMachExcServer should not dispatch the message to
  // MachMessageServerFunction, but should generate a MIG_BAD_ID error reply.

  const mach_msg_id_t unknown_ids[] = {
      // Reasonable things to check.
      -101,
      -100,
      -99,
      -1,
      0,
      1,
      99,
      100,
      101,

      // Invalid IDs right around valid ones.
      2400,
      2404,
      2408,

      // Valid and invalid IDs in the range used for replies, not requests.
      2500,
      2501,
      2502,
      2503,
      2504,
      2505,
      2506,
      2507,
      2508,
  };

  for (size_t index = 0; index < arraysize(unknown_ids); ++index) {
    mach_msg_id_t id = unknown_ids[index];

    SCOPED_TRACE(base::StringPrintf("unknown id %d", id));

    InvalidRequest request;
    EXPECT_LE(sizeof(request), server.MachMessageServerRequestSize());
    request.InitializeForTesting(id);

    BadIDErrorReply reply;
    EXPECT_LE(sizeof(reply), server.MachMessageServerReplySize());
    reply.InitializeForTesting();

    bool destroy_complex_request = false;
    EXPECT_FALSE(server.MachMessageServerFunction(
        reinterpret_cast<mach_msg_header_t*>(&request),
        reinterpret_cast<mach_msg_header_t*>(&reply),
        &destroy_complex_request));

    // The request wasn’t handled, nothing got a chance to change the value of
    // this variable. MachMessageServer would destroy the request if it was
    // complex, regardless of what was done to this variable, because the
    // return code was not KERN_SUCCESS or MIG_NO_REPLY.
    EXPECT_FALSE(destroy_complex_request);

    reply.Verify(id);
  }
}

class TestExcServerVariants : public UniversalMachExcServer,
                              public MachMultiprocess {
 public:
  TestExcServerVariants(exception_behavior_t behavior,
                        thread_state_flavor_t flavor,
                        mach_msg_type_number_t state_count)
      : UniversalMachExcServer(),
        MachMultiprocess(),
        behavior_(behavior),
        flavor_(flavor),
        state_count_(state_count),
        handled_(false) {
  }

  // UniversalMachExcServer:

  virtual kern_return_t CatchMachException(
      exception_behavior_t behavior,
      exception_handler_t exception_port,
      thread_t thread,
      task_t task,
      exception_type_t exception,
      const mach_exception_data_type_t* code,
      mach_msg_type_number_t code_count,
      thread_state_flavor_t* flavor,
      const natural_t* old_state,
      mach_msg_type_number_t old_state_count,
      thread_state_t new_state,
      mach_msg_type_number_t* new_state_count,
      bool* destroy_complex_request) override {
    *destroy_complex_request = true;

    EXPECT_FALSE(handled_);
    handled_ = true;

    EXPECT_EQ(behavior_, behavior);

    EXPECT_EQ(LocalPort(), exception_port);

    if (ExceptionBehaviorHasIdentity(behavior)) {
      EXPECT_NE(kMachPortNull, thread);
      EXPECT_EQ(ChildTask(), task);
    } else {
      EXPECT_EQ(kMachPortNull, thread);
      EXPECT_EQ(kMachPortNull, task);
    }

    EXPECT_EQ(EXC_CRASH, exception);
    EXPECT_EQ(2u, code_count);

    // The code_count check above would ideally use ASSERT_EQ so that the next
    // conditional would not be necessary, but ASSERT_* requires a function
    // returning type void, and the interface dictates otherwise here.
    if (code_count >= 1) {
      // The signal that terminated the process is stored in code[0] along with
      // some other data. See 10.9.4 xnu-2422.110.17/bsd/kern/kern_exit.c
      // proc_prepareexit().
      int sig = (code[0] >> 24) & 0xff;
      SetExpectedChildTermination(kTerminationSignal, sig);
    }

    const bool has_state = ExceptionBehaviorHasState(behavior);
    if (has_state) {
      EXPECT_EQ(flavor_, *flavor);
      EXPECT_EQ(state_count_, old_state_count);
      EXPECT_NE(static_cast<const natural_t*>(NULL), old_state);
      EXPECT_EQ(static_cast<mach_msg_type_number_t>(THREAD_STATE_MAX),
                *new_state_count);
      EXPECT_NE(static_cast<natural_t*>(NULL), new_state);
    } else {
      EXPECT_EQ(THREAD_STATE_NONE, *flavor);
      EXPECT_EQ(0u, old_state_count);
      EXPECT_EQ(NULL, old_state);
      EXPECT_EQ(0u, *new_state_count);
      EXPECT_EQ(NULL, new_state);
    }

    // Even for an EXC_CRASH handler, returning KERN_SUCCESS with a
    // state-carrying reply will cause the kernel to try to set a new thread
    // state, leading to a perceptible waste of time. Returning
    // MACH_RCV_PORT_DIED is the only way to suppress this behavior while also
    // preventing the kernel from looking for another (host-level) EXC_CRASH
    // handler. See 10.9.4 xnu-2422.110.17/osfmk/kern/exception.c
    // exception_triage().
    return has_state ? MACH_RCV_PORT_DIED : KERN_SUCCESS;
  }

 private:
  // MachMultiprocess:

  virtual void MachMultiprocessParent() override {
    kern_return_t kr = MachMessageServer::Run(this,
                                              LocalPort(),
                                              MACH_MSG_OPTION_NONE,
                                              MachMessageServer::kOneShot,
                                              MachMessageServer::kBlocking,
                                              0);
    EXPECT_EQ(KERN_SUCCESS, kr)
        << MachErrorMessage(kr, "MachMessageServer::Run");

    EXPECT_TRUE(handled_);
  }

  virtual void MachMultiprocessChild() override {
    // Set the parent as the exception handler for EXC_CRASH.
    kern_return_t kr = task_set_exception_ports(
        mach_task_self(), EXC_MASK_CRASH, RemotePort(), behavior_, flavor_);
    ASSERT_EQ(KERN_SUCCESS, kr)
        << MachErrorMessage(kr, "task_set_exception_ports");

    // Now crash.
    __builtin_trap();
  }

  exception_behavior_t behavior_;
  thread_state_flavor_t flavor_;
  mach_msg_type_number_t state_count_;
  bool handled_;

  DISALLOW_COPY_AND_ASSIGN(TestExcServerVariants);
};

TEST(ExcServerVariants, ExceptionRaise) {
  TestExcServerVariants test_exc_server_variants(
      EXCEPTION_DEFAULT, THREAD_STATE_NONE, 0);
  test_exc_server_variants.Run();
}

TEST(ExcServerVariants, ExceptionRaiseState) {
  TestExcServerVariants test_exc_server_variants(
      EXCEPTION_STATE, MACHINE_THREAD_STATE, MACHINE_THREAD_STATE_COUNT);
  test_exc_server_variants.Run();
}

TEST(ExcServerVariants, ExceptionRaiseStateIdentity) {
  TestExcServerVariants test_exc_server_variants(EXCEPTION_STATE_IDENTITY,
                                                 MACHINE_THREAD_STATE,
                                                 MACHINE_THREAD_STATE_COUNT);
  test_exc_server_variants.Run();
}

TEST(ExcServerVariants, MachExceptionRaise) {
  TestExcServerVariants test_exc_server_variants(
      MACH_EXCEPTION_CODES | EXCEPTION_DEFAULT, THREAD_STATE_NONE, 0);
  test_exc_server_variants.Run();
}

TEST(ExcServerVariants, MachExceptionRaiseState) {
  TestExcServerVariants test_exc_server_variants(
      MACH_EXCEPTION_CODES | EXCEPTION_STATE,
      MACHINE_THREAD_STATE,
      MACHINE_THREAD_STATE_COUNT);
  test_exc_server_variants.Run();
}

TEST(ExcServerVariants, MachExceptionRaiseStateIdentity) {
  TestExcServerVariants test_exc_server_variants(
      MACH_EXCEPTION_CODES | EXCEPTION_STATE_IDENTITY,
      MACHINE_THREAD_STATE,
      MACHINE_THREAD_STATE_COUNT);
  test_exc_server_variants.Run();
}

TEST(ExcServerVariants, ThreadStates) {
  // So far, all of the tests worked with MACHINE_THREAD_STATE. Now try all of
  // the other thread state flavors that are expected to work.

  struct TestData {
    thread_state_flavor_t flavor;
    mach_msg_type_number_t count;
  };
  const TestData test_data[] = {
#if defined(ARCH_CPU_X86_FAMILY)
    // For the x86 family, exception handlers can only properly receive the
    // thread, float, and exception state flavors. There’s a bug in the kernel
    // that causes it to call thread_getstatus() (a wrapper for the more
    // familiar thread_get_state()) with an incorrect state buffer size
    // parameter when delivering an exception. 10.9.4
    // xnu-2422.110.17/osfmk/kern/exception.c exception_deliver() uses the
    // _MachineStateCount[] array indexed by the flavor number to obtain the
    // buffer size. 10.9.4 xnu-2422.110.17/osfmk/i386/pcb.c contains the
    // definition of this array for the x86 family. The slots corresponding to
    // thread, float, and exception state flavors in both native-width (32- and
    // 64-bit) and universal are correct, but the remaining elements in the
    // array are not. This includes elements that would correspond to debug and
    // AVX state flavors, so these cannot be tested here.
    //
    // When machine_thread_get_state() (the machine-specific implementation of
    // thread_get_state()) encounters an undersized buffer as reported by the
    // buffer size parameter, it returns KERN_INVALID_ARGUMENT, which causes
    // exception_deliver() to not actually deliver the exception and instead
    // return that error code to exception_triage() as well.
    //
    // This bug is filed as radar 18312067.
    //
    // Additionaly, the AVX state flavors are also not tested because they’re
    // not available on all CPUs and OS versions.
#if defined(ARCH_CPU_X86)
    { x86_THREAD_STATE32, x86_THREAD_STATE32_COUNT },
    { x86_FLOAT_STATE32, x86_FLOAT_STATE32_COUNT },
    { x86_EXCEPTION_STATE32, x86_EXCEPTION_STATE32_COUNT },
#endif
#if defined(ARCH_CPU_X86_64)
    { x86_THREAD_STATE64, x86_THREAD_STATE64_COUNT },
    { x86_FLOAT_STATE64, x86_FLOAT_STATE64_COUNT },
    { x86_EXCEPTION_STATE64, x86_EXCEPTION_STATE64_COUNT },
#endif
    { x86_THREAD_STATE, x86_THREAD_STATE_COUNT },
    { x86_FLOAT_STATE, x86_FLOAT_STATE_COUNT },
    { x86_EXCEPTION_STATE, x86_EXCEPTION_STATE_COUNT },
#else
#error Port this test to your CPU architecture.
#endif
  };

  for (size_t index = 0; index < arraysize(test_data); ++index) {
    const TestData& test = test_data[index];
    SCOPED_TRACE(base::StringPrintf(
        "index %zu, flavor %d", index, test.flavor));

    TestExcServerVariants test_exc_server_variants(
        MACH_EXCEPTION_CODES | EXCEPTION_STATE_IDENTITY,
        test.flavor,
        test.count);
    test_exc_server_variants.Run();
  }
}

}  // namespace
