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

#include "util/mach/mach_message_util.h"

#include "base/basictypes.h"
#include "gtest/gtest.h"
#include "util/mach/mach_extensions.h"

namespace crashpad {
namespace test {
namespace {

TEST(MachMessageUtil, PrepareMIGReplyFromRequest_SetMIGReplyError) {
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

TEST(MachMessageUtil, MachMessageTrailerFromHeader) {
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
  EXPECT_EQ(&test.receive.trailer,
            MachMessageTrailerFromHeader(&test.receive));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
