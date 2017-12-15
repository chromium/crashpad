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

#include "util/linux/handler_client.h"

#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "util/file/file_io.h"
#include "util/linux/ptrace_broker.h"

namespace crashpad {

HandlerClient::HandlerClient(int sock)
    : server_sock_(sock), ptracer_(-1), can_set_ptracer_(true) {}

HandlerClient::~HandlerClient() = default;

int HandlerClient::RequestCrashDump(const ClientInformation& info) {
  ClientToServerMessage message;
  message.type = ClientToServerMessage::kCrashDumpRequest;
  message.client_info = info;

  iovec iov;
  iov.iov_base = &message;
  iov.iov_len = sizeof(message);

  msghdr msg;
  msg.msg_name = nullptr;
  msg.msg_namelen = 0;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  ucred creds;
  creds.pid = getpid();
  creds.uid = getuid();
  creds.gid = getgid();

  char cmsg_buf[CMSG_SPACE(sizeof(creds))];
  msg.msg_control = cmsg_buf;
  msg.msg_controllen = sizeof(cmsg_buf);

  cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_CREDENTIALS;
  cmsg->cmsg_len = CMSG_LEN(sizeof(creds));
  *reinterpret_cast<ucred*>(CMSG_DATA(cmsg)) = creds;

  if (sendmsg(server_sock_, &msg, MSG_NOSIGNAL) < 0) {
    return errno;
  }

  return 0;
}

int HandlerClient::WaitForCrashDumpComplete() {
  ServerToClientMessage message;

  while (LoggingReadFileExactly(server_sock_, &message, sizeof(message))) {
    switch (message.type) {
      case ServerToClientMessage::kTypeForkBroker: {
        LOG(INFO) << "Forking broker";
        pid_t pid = fork();
        if (pid < 0) {
          Errno error = errno;
          if (!WriteFile(server_sock_, &error, sizeof(error))) {
            return errno;
          }
          continue;
        }

        if (pid == 0) {
#if defined(ARCH_CPU_64_BITS)
          constexpr bool am_64_bit = true;
#else
          constexpr bool am_64_bit = false;
#endif  // ARCH_CPU_64_BITS

          PtraceBroker broker(server_sock_, am_64_bit);
          _exit(broker.Run());
        }

        // TODO remove this
        prctl(PR_SET_PTRACER, pid, 0, 0, 0);

        int status;
        pid_t child = HANDLE_EINTR(waitpid(pid, &status, 0));
        if (child < 0) {
          // TODO
        }
        DCHECK_EQ(child, pid);

        if (status != 0) {
          return status;
        }
        continue;
      }

      case ServerToClientMessage::kTypeSetPtracer: {
        LOG(INFO) << "Setting ptracer";
        Errno result = SetPtracer(message.pid);
        if (!WriteFile(server_sock_, &result, sizeof(result))) {
          return errno;
        }
        continue;
      }

      case ServerToClientMessage::kTypeCrashDumpComplete:
      case ServerToClientMessage::kTypeCrashDumpFailed:
        LOG(INFO) << "crash dump complete";
        return 0;
    }

    DCHECK(false);
  }

  return errno;
}

int HandlerClient::SetPtracer(pid_t pid) {
  if (ptracer_ == pid) {
    return 0;
  }

  if (!can_set_ptracer_) {
    return EPERM;
  }

  if (prctl(PR_SET_PTRACER, pid, 0, 0, 0) == 0) {
    return 0;
  }
  return errno;
}

}  // namespace crashpad
