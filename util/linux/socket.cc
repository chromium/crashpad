// Copyright 2019 The Crashpad Authors. All rights reserved.
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

#include "util/linux/socket.h"

#include <unistd.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"

namespace crashpad {

bool UnixSocketpair(ScopedFileHandle* s1, ScopedFileHandle* s2) {
  int socks[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, socks) != 0) {
    PLOG(ERROR) << "socketpair";
    return false;
  }
  s1->reset(socks[0]);
  s2->reset(socks[1]);
  return true;
}

int SendMsg(int fd, const void* buf, size_t buf_size) {
  iovec iov;
  iov.iov_base = const_cast<void*>(buf);
  iov.iov_len = buf_size;

  msghdr msg;
  msg.msg_name = nullptr;
  msg.msg_namelen = 0;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  ucred creds;
  creds.pid = getpid();
  creds.uid = geteuid();
  creds.gid = getegid();

  char cmsg_buf[CMSG_SPACE(sizeof(creds))];
  msg.msg_control = cmsg_buf;
  msg.msg_controllen = sizeof(cmsg_buf);

  cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_CREDENTIALS;
  cmsg->cmsg_len = CMSG_LEN(sizeof(creds));
  *reinterpret_cast<ucred*>(CMSG_DATA(cmsg)) = creds;

  if (HANDLE_EINTR(sendmsg(fd, &msg, MSG_NOSIGNAL)) < 0) {
    PLOG(ERROR) << "sendmsg";
    return errno;
  }
  return 0;
}

bool RecvMsg(int fd, void* buf, size_t buf_size, ucred* creds) {
  iovec iov;
  iov.iov_base = buf;
  iov.iov_len = buf_size;

  msghdr msg;
  msg.msg_name = nullptr;
  msg.msg_namelen = 0;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  char cmsg_buf[CMSG_SPACE(sizeof(ucred))];
  msg.msg_control = cmsg_buf;
  msg.msg_controllen = sizeof(cmsg_buf);
  msg.msg_flags = 0;

  int res = HANDLE_EINTR(recvmsg(fd, &msg, 0));
  if (res < 0) {
    PLOG(ERROR) << "recvmsg";
    return false;
  }
  if (res == 0) {
    // The sender had an orderly shutdown.
    return false;
  }

  ucred* local_creds = nullptr;

  for (cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg;
       cmsg = CMSG_NXTHDR(&msg, cmsg)) {
    if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
      int* fdp = reinterpret_cast<int*>(CMSG_DATA(cmsg));
      size_t fd_count = (reinterpret_cast<char*>(cmsg) + cmsg->cmsg_len -
                         reinterpret_cast<char*>(fdp)) /
                        sizeof(int);
      for (size_t index = 0; index < fd_count; ++index) {
        if (HANDLE_EINTR(close(fdp[index])) != 0) {
          PLOG(ERROR) << "close";
        }
      }
      continue;
    }

    if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_CREDENTIALS) {
      DCHECK(!local_creds);
      local_creds = reinterpret_cast<ucred*>(CMSG_DATA(cmsg));
      continue;
    }

    LOG(ERROR) << "unhandled cmsg";
    return false;
  }

  if (msg.msg_name != nullptr || msg.msg_namelen != 0) {
    LOG(ERROR) << "unexpected msg name";
    return false;
  }

  if (msg.msg_flags & MSG_TRUNC || msg.msg_flags & MSG_CTRUNC) {
    LOG(ERROR) << "truncated msg";
    return false;
  }

  if (!local_creds) {
    LOG(ERROR) << "missing credentials";
    return false;
  }
  *creds = *local_creds;

  return true;
}

}  // namespace crashpad
