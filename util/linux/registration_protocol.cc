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

#include "util/linux/registration_protocol.h"

#include <sys/socket.h>

#include "base/logging.h"
#include "util/file/file_io.h"

namespace crashpad {

bool RegisterWithHandler(int server_sock,
                         const RegistrationRequest& registration,
                         int registration_socket) {
  ClientToServerMessage message;
  message.type = ClientToServerMessage::kRegistration;
  message.registration = registration;

  iovec iov;
  iov.iov_base = &message;
  iov.iov_len = sizeof(message);

  msghdr msg;
  msg.msg_name = nullptr;
  msg.msg_namelen = 0;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  char cmsg_buf[CMSG_SPACE(sizeof(int))];
  msg.msg_control = cmsg_buf;
  msg.msg_controllen = sizeof(cmsg_buf);

  cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_RIGHTS;
  cmsg->cmsg_len = CMSG_LEN(sizeof(int));
  *reinterpret_cast<int*>(CMSG_DATA(cmsg)) = registration_socket;

  if (sendmsg(server_sock, &msg, MSG_NOSIGNAL) <= 0) {
    PLOG(ERROR) << "sendmsg";
    return false;
  }

  bool success;
  LoggingReadFileExactly(server_sock, &success, sizeof(success));
  return success;
}

bool ShutdownHandler(int server_sock) {
  ClientToServerMessage message;
  message.type = ClientToServerMessage::kShutdown;

  iovec iov;
  iov.iov_base = &message;
  iov.iov_len = sizeof(message);

  msghdr msg;
  msg.msg_name = nullptr;
  msg.msg_namelen = 0;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = nullptr;
  msg.msg_controllen = 0;

  if (sendmsg(server_sock, &msg, MSG_NOSIGNAL) <= 0) {
    PLOG(ERROR) << "sendmsg";
    return false;
  }

  return true;
}

}  // namespace crashpad
