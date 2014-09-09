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

#include "util/mach/mach_message_server.h"

#include <mach/mach.h>
#include <string.h>

#include "base/basictypes.h"
#include "base/mac/scoped_mach_port.h"
#include "gtest/gtest.h"
#include "util/file/fd_io.h"
#include "util/test/errors.h"
#include "util/test/mac/mach_errors.h"
#include "util/test/mac/mach_multiprocess.h"

namespace {

using namespace crashpad;
using namespace crashpad::test;

class TestMachMessageServer : public MachMessageServer::Interface,
                              public MachMultiprocess {
 public:
  struct Options {
    // The type of reply port that the client should put in its request message.
    enum ReplyPortType {
      // The normal reply port is the client’s local port, to which it holds
      // a receive right. This allows the server to respond directly to the
      // client. The client will expect a reply.
      kReplyPortNormal,

      // Use MACH_PORT_NULL as the reply port, which the server should detect
      // avoid attempting to send a message to, and return success. The client
      // will not expect a reply.
      kReplyPortNull,

      // Make the server see the reply port as a dead name by setting the reply
      // port to a receive right and then destroying that right before the
      // server processes the request. The server should return
      // MACH_SEND_INVALID_DEST, and the client will not expect a reply.
      kReplyPortDead,
    };

    Options()
        : expect_server_interface_method_called(true),
          parent_wait_for_child_pipe(false),
          server_persistent(MachMessageServer::kOneShot),
          server_nonblocking(MachMessageServer::kBlocking),
          server_timeout_ms(MACH_MSG_TIMEOUT_NONE),
          server_mig_retcode(KERN_SUCCESS),
          expect_server_result(KERN_SUCCESS),
          client_send_request_count(1),
          client_reply_port_type(kReplyPortNormal),
          child_send_all_requests_before_receiving_any_replies(false) {
    }

    // true if MachMessageServerFunction() is expected to be called.
    bool expect_server_interface_method_called;

    // true if the parent should wait for the child to write a byte to the pipe
    // as a signal that the child is ready for the parent to begin its side of
    // the test. This is used for nonblocking tests, which require that there
    // be something in the server’s queue before attempting a nonblocking
    // receive if the receive is to be successful.
    bool parent_wait_for_child_pipe;

    // Whether the server should run in one-shot or persistent mode.
    MachMessageServer::Persistent server_persistent;

    // Whether the server should run in blocking or nonblocking mode.
    MachMessageServer::Nonblocking server_nonblocking;

    // The server’s timeout.
    mach_msg_timeout_t server_timeout_ms;

    // The return code that the server returns to the client via the
    // mig_reply_error_t::RetCode field. A client would normally see this as
    // a Mach RPC return value.
    kern_return_t server_mig_retcode;

    // The expected return value from MachMessageServer::Run().
    kern_return_t expect_server_result;

    // The number of requests that the client should send to the server.
    size_t client_send_request_count;

    // The type of reply port that the client should provide in its request’s
    // mach_msg_header_t::msgh_local_port, which will appear to the server as
    // mach_msg_header_t::msgh_remote_port.
    ReplyPortType client_reply_port_type;

    // true if the client should send all requests before attempting to receive
    // any replies from the server. This is used for the persistent nonblocking
    // test, which requires the client to fill the server’s queue before the
    // server can attempt processing it.
    bool child_send_all_requests_before_receiving_any_replies;
  };

  explicit TestMachMessageServer(const Options& options)
      : MachMessageServer::Interface(),
        MachMultiprocess(),
        options_(options) {
  }

  // Runs the test.
  void Test() {
    EXPECT_EQ(requests_, replies_);
    uint32_t start = requests_;

    Run();

    EXPECT_EQ(requests_, replies_);
    EXPECT_EQ(options_.client_send_request_count, requests_ - start);
  }

  // MachMessageServerInterface:

  virtual bool MachMessageServerFunction(
      mach_msg_header_t* in,
      mach_msg_header_t* out,
      bool* destroy_complex_request) override {
    *destroy_complex_request = true;

    EXPECT_TRUE(options_.expect_server_interface_method_called);
    if (!options_.expect_server_interface_method_called) {
      return false;
    }

    struct ReceiveRequestMessage : public RequestMessage {
      mach_msg_trailer_t trailer;
    };

    const ReceiveRequestMessage* request =
        reinterpret_cast<ReceiveRequestMessage*>(in);
    EXPECT_EQ(static_cast<mach_msg_bits_t>(
        MACH_MSGH_BITS(MACH_MSG_TYPE_MOVE_SEND, MACH_MSG_TYPE_MOVE_SEND)),
        request->header.msgh_bits);
    EXPECT_EQ(sizeof(RequestMessage), request->header.msgh_size);
    if (options_.client_reply_port_type == Options::kReplyPortNormal) {
      EXPECT_EQ(RemotePort(), request->header.msgh_remote_port);
    }
    EXPECT_EQ(LocalPort(), request->header.msgh_local_port);
    EXPECT_EQ(kRequestMessageId, request->header.msgh_id);
    EXPECT_EQ(0, memcmp(&request->ndr, &NDR_record, sizeof(NDR_record)));
    EXPECT_EQ(requests_, request->number);
    EXPECT_EQ(static_cast<mach_msg_trailer_type_t>(MACH_MSG_TRAILER_FORMAT_0),
              request->trailer.msgh_trailer_type);
    EXPECT_EQ(MACH_MSG_TRAILER_MINIMUM_SIZE,
              request->trailer.msgh_trailer_size);

    ++requests_;

    ReplyMessage* reply = reinterpret_cast<ReplyMessage*>(out);
    reply->Head.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
    reply->Head.msgh_size = sizeof(*reply);
    reply->Head.msgh_remote_port = request->header.msgh_remote_port;
    reply->Head.msgh_local_port = MACH_PORT_NULL;
    reply->Head.msgh_id = kReplyMessageId;
    reply->NDR = NDR_record;
    reply->RetCode = options_.server_mig_retcode;
    reply->number = replies_++;

    return true;
  }

  virtual mach_msg_size_t MachMessageServerRequestSize() override {
    return sizeof(RequestMessage);
  }

  virtual mach_msg_size_t MachMessageServerReplySize() override {
    return sizeof(ReplyMessage);
  }

 private:
  struct RequestMessage {
    mach_msg_header_t header;
    NDR_record_t ndr;
    uint32_t number;
  };

  struct ReplyMessage : public mig_reply_error_t {
    uint32_t number;
  };

  // MachMultiprocess:

  virtual void MachMultiprocessParent() override {
    if (options_.parent_wait_for_child_pipe) {
      // Wait until the child is done sending what it’s going to send.
      char c;
      ssize_t rv = ReadFD(ReadPipeFD(), &c, 1);
      EXPECT_EQ(1, rv) << ErrnoMessage("read");
      EXPECT_EQ('\0', c);
    }

    kern_return_t kr;
    ASSERT_EQ(options_.expect_server_result,
              (kr = MachMessageServer::Run(this,
                                           LocalPort(),
                                           MACH_MSG_OPTION_NONE,
                                           options_.server_persistent,
                                           options_.server_nonblocking,
                                           options_.server_timeout_ms)))
        << MachErrorMessage(kr, "MachMessageServer");
  }

  virtual void MachMultiprocessChild() override {
    for (size_t index = 0;
         index < options_.client_send_request_count;
         ++index) {
      if (options_.child_send_all_requests_before_receiving_any_replies) {
        // For this test, all of the messages need to go into the queue before
        // the parent is allowed to start processing them. Don’t attempt to
        // process replies before all of the requests are sent, because the
        // server won’t have sent any replies until all of the requests are in
        // its queue.
        ChildSendRequest();
      } else {
        ChildSendRequestAndWaitForReply();
      }
      if (testing::Test::HasFatalFailure()) {
        return;
      }
    }

    if (options_.parent_wait_for_child_pipe &&
        options_.child_send_all_requests_before_receiving_any_replies) {
      // Now that all of the requests have been sent, let the parent know that
      // it’s safe to begin processing them, and then wait for the replies.
      ChildNotifyParentViaPipe();
      if (testing::Test::HasFatalFailure()) {
        return;
      }

      for (size_t index = 0;
           index < options_.client_send_request_count;
           ++index) {
        ChildWaitForReply();
        if (testing::Test::HasFatalFailure()) {
          return;
        }
      }
    }
  }

  // In the child process, sends a request message to the server.
  void ChildSendRequest() {
    // local_receive_port_owner will the receive right that is created in this
    // scope and intended to be destroyed when leaving this scope, after it has
    // been carried in a Mach message.
    base::mac::ScopedMachReceiveRight local_receive_port_owner;

    RequestMessage request = {};
    request.header.msgh_bits =
        MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND);
    request.header.msgh_size = sizeof(request);
    request.header.msgh_remote_port = RemotePort();
    kern_return_t kr;
    switch (options_.client_reply_port_type) {
      case Options::kReplyPortNormal:
        request.header.msgh_local_port = LocalPort();
        break;
      case Options::kReplyPortNull:
        request.header.msgh_local_port = MACH_PORT_NULL;
        break;
      case Options::kReplyPortDead: {
        // Use a newly-allocated receive right that will be destroyed when this
        // method returns. A send right will be made from this receive right and
        // carried in the request message to the server. By the time the server
        // looks at the right, it will have become a dead name.
        kr = mach_port_allocate(mach_task_self(),
                                MACH_PORT_RIGHT_RECEIVE,
                                &request.header.msgh_local_port);
        ASSERT_EQ(KERN_SUCCESS, kr)
            << MachErrorMessage(kr, "mach_port_allocate");
        local_receive_port_owner.reset(request.header.msgh_local_port);
        break;
      }
    }
    request.header.msgh_id = kRequestMessageId;
    request.number = requests_++;
    request.ndr = NDR_record;

    kr = mach_msg(&request.header,
                  MACH_SEND_MSG | MACH_SEND_TIMEOUT,
                  request.header.msgh_size,
                  0,
                  MACH_PORT_NULL,
                  MACH_MSG_TIMEOUT_NONE,
                  MACH_PORT_NULL);
    ASSERT_EQ(MACH_MSG_SUCCESS, kr) << MachErrorMessage(kr, "mach_msg");
  }

  // In the child process, waits for a reply message from the server.
  void ChildWaitForReply() {
    if (options_.client_reply_port_type != Options::kReplyPortNormal) {
      // The client shouldn’t expect a reply when it didn’t send a good reply
      // port with its request.
      return;
    }

    struct ReceiveReplyMessage : public ReplyMessage {
      mach_msg_trailer_t trailer;
    };

    ReceiveReplyMessage reply = {};
    kern_return_t kr = mach_msg(&reply.Head,
                                MACH_RCV_MSG,
                                0,
                                sizeof(reply),
                                LocalPort(),
                                MACH_MSG_TIMEOUT_NONE,
                                MACH_PORT_NULL);
    ASSERT_EQ(MACH_MSG_SUCCESS, kr) << MachErrorMessage(kr, "mach_msg");

    ASSERT_EQ(static_cast<mach_msg_bits_t>(
        MACH_MSGH_BITS(0, MACH_MSG_TYPE_MOVE_SEND)), reply.Head.msgh_bits);
    ASSERT_EQ(sizeof(ReplyMessage), reply.Head.msgh_size);
    ASSERT_EQ(static_cast<mach_port_t>(MACH_PORT_NULL),
              reply.Head.msgh_remote_port);
    ASSERT_EQ(LocalPort(), reply.Head.msgh_local_port);
    ASSERT_EQ(kReplyMessageId, reply.Head.msgh_id);
    ASSERT_EQ(0, memcmp(&reply.NDR, &NDR_record, sizeof(NDR_record)));
    ASSERT_EQ(options_.server_mig_retcode, reply.RetCode);
    ASSERT_EQ(replies_, reply.number);
    ASSERT_EQ(static_cast<mach_msg_trailer_type_t>(MACH_MSG_TRAILER_FORMAT_0),
              reply.trailer.msgh_trailer_type);
    ASSERT_EQ(MACH_MSG_TRAILER_MINIMUM_SIZE, reply.trailer.msgh_trailer_size);

    ++replies_;
  }

  // For test types where the child needs to notify the server in the parent
  // that the child is ready, this method will send a byte via the POSIX pipe.
  // The parent will be waiting in a read() on this pipe, and will proceed to
  // running MachMessageServer() once it’s received.
  void ChildNotifyParentViaPipe() {
    char c = '\0';
    ssize_t rv = WriteFD(WritePipeFD(), &c, 1);
    ASSERT_EQ(1, rv) << ErrnoMessage("write");
  }

  // In the child process, sends a request message to the server and then
  // receives a reply message.
  void ChildSendRequestAndWaitForReply() {
    ChildSendRequest();
    if (testing::Test::HasFatalFailure()) {
      return;
    }

    if (options_.parent_wait_for_child_pipe &&
        !options_.child_send_all_requests_before_receiving_any_replies) {
      // The parent is waiting to read a byte to indicate that the message has
      // been placed in the queue.
      ChildNotifyParentViaPipe();
      if (testing::Test::HasFatalFailure()) {
        return;
      }
    }

    ChildWaitForReply();
  }

  const Options& options_;

  static uint32_t requests_;
  static uint32_t replies_;

  static const mach_msg_id_t kRequestMessageId = 16237;
  static const mach_msg_id_t kReplyMessageId = kRequestMessageId + 100;

  DISALLOW_COPY_AND_ASSIGN(TestMachMessageServer);
};

uint32_t TestMachMessageServer::requests_;
uint32_t TestMachMessageServer::replies_;
const mach_msg_id_t TestMachMessageServer::kRequestMessageId;
const mach_msg_id_t TestMachMessageServer::kReplyMessageId;

TEST(MachMessageServer, Basic) {
  // The client sends one message to the server, which will wait indefinitely in
  // blocking mode for it.
  TestMachMessageServer::Options options;
  TestMachMessageServer test_mach_message_server(options);
  test_mach_message_server.Test();
}

TEST(MachMessageServer, NonblockingNoMessage) {
  // The server waits in nonblocking mode and the client sends nothing, so the
  // server should return immediately without processing any message.
  TestMachMessageServer::Options options;
  options.expect_server_interface_method_called = false;
  options.server_nonblocking = MachMessageServer::kNonblocking;
  options.expect_server_result = MACH_RCV_TIMED_OUT;
  options.client_send_request_count = 0;
  TestMachMessageServer test_mach_message_server(options);
  test_mach_message_server.Test();
}

TEST(MachMessageServer, TimeoutNoMessage) {
  // The server waits in blocking mode for one message, but with a timeout. The
  // client sends no message, so the server returns after the timeout.
  TestMachMessageServer::Options options;
  options.expect_server_interface_method_called = false;
  options.server_timeout_ms = 10;
  options.expect_server_result = MACH_RCV_TIMED_OUT;
  options.client_send_request_count = 0;
  TestMachMessageServer test_mach_message_server(options);
  test_mach_message_server.Test();
}

TEST(MachMessageServer, Nonblocking) {
  // The client sends one message to the server and then signals the server that
  // it’s safe to start waiting for it in nonblocking mode. The message is in
  // the server’s queue, so it’s able to receive it when it begins listening in
  // nonblocking mode.
  TestMachMessageServer::Options options;
  options.parent_wait_for_child_pipe = true;
  options.server_nonblocking = MachMessageServer::kNonblocking;
  TestMachMessageServer test_mach_message_server(options);
  test_mach_message_server.Test();
}

TEST(MachMessageServer, Timeout) {
  // The client sends one message to the server, which will wait in blocking
  // mode for it up to a specific timeout.
  TestMachMessageServer::Options options;
  options.server_timeout_ms = 10;
  TestMachMessageServer test_mach_message_server(options);
  test_mach_message_server.Test();
}

TEST(MachMessageServer, PersistentTenMessages) {
  // The server waits for as many messages as it can receive in blocking mode
  // with a timeout. The client sends several messages, and the server processes
  // them all.
  TestMachMessageServer::Options options;
  options.server_persistent = MachMessageServer::kPersistent;
  options.server_timeout_ms = 10;
  options.expect_server_result = MACH_RCV_TIMED_OUT;
  options.client_send_request_count = 10;
  TestMachMessageServer test_mach_message_server(options);
  test_mach_message_server.Test();
}

TEST(MachMessageServer, PersistentNonblockingFourMessages) {
  // The client sends several messages to the server and then signals the server
  // that it’s safe to start waiting for them in nonblocking mode. The server
  // then listens for them in nonblocking persistent mode, and receives all of
  // them because they’ve been queued up. The client doesn’t wait for the
  // replies until after it’s put all of its requests into the server’s queue.
  //
  // This test is sensitive to the length of the IPC queue limit. Mach ports
  // normally have a queue length limit of MACH_PORT_QLIMIT_DEFAULT (which is
  // MACH_PORT_QLIMIT_BASIC, or 5). The number of messages sent for this test
  // must be below this, because the server does not begin dequeueing request
  // messages until the client has finished sending them.
  TestMachMessageServer::Options options;
  options.parent_wait_for_child_pipe = true;
  options.server_persistent = MachMessageServer::kPersistent;
  options.server_nonblocking = MachMessageServer::kNonblocking;
  options.expect_server_result = MACH_RCV_TIMED_OUT;
  options.client_send_request_count = 4;
  options.child_send_all_requests_before_receiving_any_replies = true;
  TestMachMessageServer test_mach_message_server(options);
  test_mach_message_server.Test();
}

TEST(MachMessageServer, ReturnCodeInvalidArgument) {
  // This tests that the mig_reply_error_t::RetCode field is properly returned
  // to the client.
  TestMachMessageServer::Options options;
  TestMachMessageServer test_mach_message_server(options);
  options.server_mig_retcode = KERN_INVALID_ARGUMENT;
  test_mach_message_server.Test();
}

TEST(MachMessageServer, ReplyPortNull) {
  // The client sets its reply port to MACH_PORT_NULL. The server should see
  // this and avoid sending a message to the null port. No reply message is
  // sent and the server returns success.
  TestMachMessageServer::Options options;
  TestMachMessageServer test_mach_message_server(options);
  options.client_reply_port_type =
      TestMachMessageServer::Options::kReplyPortNull;
  test_mach_message_server.Test();
}

TEST(MachMessageServer, ReplyPortDead) {
  // The client allocates a new port and uses it as the reply port in its
  // request message, and then deallocates its receive right to that port. It
  // then signals the server to process the request message. The server’s view
  // of the port is that it is a dead name. The server function will return
  // MACH_SEND_INVALID_DEST because it’s not possible to send a message to a
  // dead name.
  TestMachMessageServer::Options options;
  TestMachMessageServer test_mach_message_server(options);
  options.parent_wait_for_child_pipe = true;
  options.expect_server_result = MACH_SEND_INVALID_DEST;
  options.client_reply_port_type =
      TestMachMessageServer::Options::kReplyPortDead;
  test_mach_message_server.Test();
}

}  // namespace
