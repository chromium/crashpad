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

#include "util/mach/bootstrap.h"

#include <mach/mach.h>
#include <servers/bootstrap.h>
#include <string.h>

#include <algorithm>
#include <string>

#include "base/basictypes.h"
#include "base/mac/scoped_mach_port.h"
#include "base/rand_util.h"
#include "gtest/gtest.h"
#include "util/mach/mach_extensions.h"
#include "util/test/mac/mach_errors.h"

namespace {

using namespace crashpad;
using namespace crashpad::test;
using namespace testing;

TEST(Bootstrap, BootstrapCheckIn) {
  std::string service_name = "com.googlecode.crashpad.test.bootstrap.";
  for (int index = 0; index < 16; ++index) {
    service_name.append(1, base::RandInt('A', 'Z'));
  }

  // The service shouldn’t be registered in the bootstrap namespace yet.
  mach_port_t client_port = MACH_PORT_NULL;
  kern_return_t kr =
      bootstrap_look_up(bootstrap_port, service_name.c_str(), &client_port);
  ASSERT_EQ(BOOTSTRAP_UNKNOWN_SERVICE, kr)
      << BootstrapErrorMessage(kr, "bootstrap_look_up");

  // Check in, getting a receive right.
  mach_port_t server_port;
  kr = BootstrapCheckIn(bootstrap_port, service_name.c_str(), &server_port);
  ASSERT_EQ(BOOTSTRAP_SUCCESS, kr)
      << BootstrapErrorMessage(kr, "bootstrap_check_in");
  ASSERT_NE(kMachPortNull, server_port);
  base::mac::ScopedMachReceiveRight server_port_owner(server_port);

  // A subsequent checkin attempt should fail.
  mach_port_t fail_port = MACH_PORT_NULL;
  kr = BootstrapCheckIn(bootstrap_port, service_name.c_str(), &fail_port);
  EXPECT_EQ(BOOTSTRAP_SERVICE_ACTIVE, kr);
  EXPECT_EQ(kMachPortNull, fail_port);

  // Look up the service, getting a send right.
  kr = bootstrap_look_up(bootstrap_port, service_name.c_str(), &client_port);
  ASSERT_EQ(BOOTSTRAP_SUCCESS, kr)
      << BootstrapErrorMessage(kr, "bootstrap_look_up");
  base::mac::ScopedMachSendRight client_port_owner(client_port);
  EXPECT_NE(kMachPortNull, client_port);

  // Have the “client” send a message to the “server”.
  struct SendMessage {
    mach_msg_header_t header;
    char data[64];
  };
  SendMessage send_message = {};
  send_message.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
  send_message.header.msgh_size = sizeof(send_message);
  send_message.header.msgh_remote_port = client_port;
  const char kMessageData[] = "Mach messaging is not a big truck";
  const size_t message_data_size =
      std::min(sizeof(kMessageData), sizeof(send_message.data));
  memcpy(send_message.data, kMessageData, message_data_size);

  // The receive operation happens in this same thread after the send, so use a
  // non-blocking send (MACH_SEND_TIMEOUT with MACH_MSG_TIMEOUT_NONE). This is a
  // small and simple message and the destination port’s queue should be empty
  // so a non-blocking send should be successful.
  kr = mach_msg(&send_message.header,
                MACH_SEND_MSG | MACH_SEND_TIMEOUT,
                send_message.header.msgh_size,
                0,
                MACH_PORT_NULL,
                MACH_MSG_TIMEOUT_NONE,
                MACH_PORT_NULL);
  ASSERT_EQ(MACH_MSG_SUCCESS, kr) << MachErrorMessage(kr, "mach_msg");

  struct ReceiveMessage : public SendMessage {
    mach_msg_trailer_t trailer;
  };
  ReceiveMessage receive_message;
  memset(&receive_message, 0xa5, sizeof(receive_message));

  kr = mach_msg(&receive_message.header,
                MACH_RCV_MSG | MACH_RCV_TIMEOUT,
                0,
                sizeof(receive_message),
                server_port,
                MACH_MSG_TIMEOUT_NONE,
                MACH_PORT_NULL);
  ASSERT_EQ(MACH_MSG_SUCCESS, kr) << MachErrorMessage(kr, "mach_msg");

  EXPECT_EQ(sizeof(send_message), receive_message.header.msgh_size);
  EXPECT_EQ(0, memcmp(receive_message.data, kMessageData, message_data_size));

  // Make sure that the bootstrap server’s mapping disappears if the service
  // does.
  server_port_owner.reset();
  server_port = MACH_PORT_NULL;

  // With the server port gone, the client port should have become a dead name.
  mach_port_type_t client_port_type;
  kr = mach_port_type(mach_task_self(), client_port, &client_port_type);
  ASSERT_EQ(KERN_SUCCESS, kr) << MachErrorMessage(kr, "mach_port_type");
  EXPECT_EQ(MACH_PORT_TYPE_DEAD_NAME, client_port_type);

  client_port_owner.reset();
  client_port = MACH_PORT_NULL;

  kr = bootstrap_look_up(bootstrap_port, service_name.c_str(), &client_port);
  ASSERT_EQ(BOOTSTRAP_UNKNOWN_SERVICE, kr)
      << BootstrapErrorMessage(kr, "bootstrap_look_up");
}

}  // namespace
