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

#include "util/mach/mach_message.h"

#include <unistd.h>

#include "base/mac/scoped_mach_port.h"
#include "gtest/gtest.h"
#include "test/mac/mach_errors.h"
#include "util/mach/mach_extensions.h"
#include "util/misc/implicit_cast.h"

namespace crashpad {
namespace test {
namespace {

TEST(MachMessage, MachMessageDeadlineFromTimeout) {
  MachMessageDeadline deadline_0 =
      MachMessageDeadlineFromTimeout(kMachMessageTimeoutNonblocking);
  EXPECT_EQ(kMachMessageDeadlineNonblocking, deadline_0);

  deadline_0 =
      MachMessageDeadlineFromTimeout(kMachMessageTimeoutWaitIndefinitely);
  EXPECT_EQ(kMachMessageDeadlineWaitIndefinitely, deadline_0);

  deadline_0 = MachMessageDeadlineFromTimeout(1);
  MachMessageDeadline deadline_1 = MachMessageDeadlineFromTimeout(100);

  EXPECT_NE(kMachMessageDeadlineNonblocking, deadline_0);
  EXPECT_NE(kMachMessageDeadlineWaitIndefinitely, deadline_0);
  EXPECT_NE(kMachMessageDeadlineNonblocking, deadline_1);
  EXPECT_NE(kMachMessageDeadlineWaitIndefinitely, deadline_1);
  EXPECT_GE(deadline_1, deadline_0);
}

TEST(MachMessage, PrepareMIGReplyFromRequest_SetMIGReplyError) {
  mach_msg_header_t request;
  request.msgh_bits =
      MACH_MSGH_BITS_COMPLEX |
      MACH_MSGH_BITS(MACH_MSG_TYPE_PORT_SEND_ONCE, MACH_MSG_TYPE_PORT_SEND);
  request.msgh_size = 64;
  request.msgh_remote_port = 0x01234567;
  request.msgh_local_port = 0x89abcdef;
  request.msgh_reserved = 0xa5a5a5a5;
  request.msgh_id = 1011;

  mig_reply_error_t reply;

  // PrepareMIGReplyFromRequest() doesn’t touch this field.
  reply.RetCode = MIG_TYPE_ERROR;

  PrepareMIGReplyFromRequest(&request, &reply.Head);

  EXPECT_EQ(implicit_cast<mach_msg_bits_t>(
                MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND_ONCE, 0)),
            reply.Head.msgh_bits);
  EXPECT_EQ(sizeof(reply), reply.Head.msgh_size);
  EXPECT_EQ(request.msgh_remote_port, reply.Head.msgh_remote_port);
  EXPECT_EQ(kMachPortNull, reply.Head.msgh_local_port);
  EXPECT_EQ(0u, reply.Head.msgh_reserved);
  EXPECT_EQ(1111, reply.Head.msgh_id);
  EXPECT_EQ(NDR_record.mig_vers, reply.NDR.mig_vers);
  EXPECT_EQ(NDR_record.if_vers, reply.NDR.if_vers);
  EXPECT_EQ(NDR_record.reserved1, reply.NDR.reserved1);
  EXPECT_EQ(NDR_record.mig_encoding, reply.NDR.mig_encoding);
  EXPECT_EQ(NDR_record.int_rep, reply.NDR.int_rep);
  EXPECT_EQ(NDR_record.char_rep, reply.NDR.char_rep);
  EXPECT_EQ(NDR_record.float_rep, reply.NDR.float_rep);
  EXPECT_EQ(NDR_record.reserved2, reply.NDR.reserved2);
  EXPECT_EQ(MIG_TYPE_ERROR, reply.RetCode);

  SetMIGReplyError(&reply.Head, MIG_BAD_ID);

  EXPECT_EQ(MIG_BAD_ID, reply.RetCode);
}

TEST(MachMessage, MachMessageTrailerFromHeader) {
  mach_msg_empty_t empty;
  empty.send.header.msgh_size = sizeof(mach_msg_empty_send_t);
  EXPECT_EQ(&empty.rcv.trailer,
            MachMessageTrailerFromHeader(&empty.rcv.header));

  struct TestSendMessage : public mach_msg_header_t {
    uint8_t data[126];
  };
  struct TestReceiveMessage : public TestSendMessage {
    mach_msg_trailer_t trailer;
  };
  union TestMessage {
    TestSendMessage send;
    TestReceiveMessage receive;
  };

  TestMessage test;
  test.send.msgh_size = sizeof(TestSendMessage);
  EXPECT_EQ(&test.receive.trailer, MachMessageTrailerFromHeader(&test.receive));
}

TEST(MachMessage, AuditPIDFromMachMessageTrailer) {
  base::mac::ScopedMachReceiveRight port(NewMachPort(MACH_PORT_RIGHT_RECEIVE));
  ASSERT_NE(kMachPortNull, port);

  mach_msg_empty_send_t send = {};
  send.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_MAKE_SEND_ONCE, 0);
  send.header.msgh_size = sizeof(send);
  send.header.msgh_remote_port = port.get();
  mach_msg_return_t mr =
      MachMessageWithDeadline(&send.header,
                              MACH_SEND_MSG,
                              0,
                              MACH_PORT_NULL,
                              kMachMessageDeadlineNonblocking,
                              MACH_PORT_NULL,
                              false);
  ASSERT_EQ(MACH_MSG_SUCCESS, mr)
      << MachErrorMessage(mr, "MachMessageWithDeadline send");

  struct EmptyReceiveMessageWithAuditTrailer : public mach_msg_empty_send_t {
    union {
      mach_msg_trailer_t trailer;
      mach_msg_audit_trailer_t audit_trailer;
    };
  };

  EmptyReceiveMessageWithAuditTrailer receive;
  mr = MachMessageWithDeadline(&receive.header,
                               MACH_RCV_MSG | kMachMessageReceiveAuditTrailer,
                               sizeof(receive),
                               port.get(),
                               kMachMessageDeadlineNonblocking,
                               MACH_PORT_NULL,
                               false);
  ASSERT_EQ(MACH_MSG_SUCCESS, mr)
      << MachErrorMessage(mr, "MachMessageWithDeadline receive");

  EXPECT_EQ(getpid(), AuditPIDFromMachMessageTrailer(&receive.trailer));
}

TEST(MachMessage, MachMessageDestroyReceivedPort) {
  mach_port_t port = NewMachPort(MACH_PORT_RIGHT_RECEIVE);
  ASSERT_NE(kMachPortNull, port);
  EXPECT_TRUE(MachMessageDestroyReceivedPort(port, MACH_MSG_TYPE_PORT_RECEIVE));

  base::mac::ScopedMachReceiveRight receive(
      NewMachPort(MACH_PORT_RIGHT_RECEIVE));
  mach_msg_type_name_t right_type;
  kern_return_t kr = mach_port_extract_right(mach_task_self(),
                                             receive.get(),
                                             MACH_MSG_TYPE_MAKE_SEND,
                                             &port,
                                             &right_type);
  ASSERT_EQ(KERN_SUCCESS, kr)
      << MachErrorMessage(kr, "mach_port_extract_right");
  ASSERT_EQ(receive, port);
  ASSERT_EQ(implicit_cast<mach_msg_type_name_t>(MACH_MSG_TYPE_PORT_SEND),
            right_type);
  EXPECT_TRUE(MachMessageDestroyReceivedPort(port, MACH_MSG_TYPE_PORT_SEND));

  kr = mach_port_extract_right(mach_task_self(),
                               receive.get(),
                               MACH_MSG_TYPE_MAKE_SEND_ONCE,
                               &port,
                               &right_type);
  ASSERT_EQ(KERN_SUCCESS, kr)
      << MachErrorMessage(kr, "mach_port_extract_right");
  ASSERT_NE(kMachPortNull, port);
  EXPECT_NE(receive, port);
  ASSERT_EQ(implicit_cast<mach_msg_type_name_t>(MACH_MSG_TYPE_PORT_SEND_ONCE),
            right_type);
  EXPECT_TRUE(
      MachMessageDestroyReceivedPort(port, MACH_MSG_TYPE_PORT_SEND_ONCE));

  kr = mach_port_extract_right(mach_task_self(),
                               receive.get(),
                               MACH_MSG_TYPE_MAKE_SEND,
                               &port,
                               &right_type);
  ASSERT_EQ(KERN_SUCCESS, kr)
      << MachErrorMessage(kr, "mach_port_extract_right");
  ASSERT_EQ(receive, port);
  ASSERT_EQ(implicit_cast<mach_msg_type_name_t>(MACH_MSG_TYPE_PORT_SEND),
            right_type);
  EXPECT_TRUE(MachMessageDestroyReceivedPort(port, MACH_MSG_TYPE_PORT_RECEIVE));
  ignore_result(receive.release());
  EXPECT_TRUE(MachMessageDestroyReceivedPort(port, MACH_MSG_TYPE_PORT_SEND));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
