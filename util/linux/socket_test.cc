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

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "gtest/gtest.h"
#include "test/multiprocess_exec.h"
#include "third_party/lss/lss.h"
#include "util/linux/socket.h"
#include "util/synchronization/semaphore.h"
#include "util/thread/thread.h"

namespace crashpad {
namespace test {
namespace {

bool SetSoPasscred(int fd) {
  int optval = 1;
  socklen_t optlen = sizeof(optval);
  if (setsockopt(fd, SOL_SOCKET, SO_PASSCRED, &optval, optlen) !=
      0) {
    PLOG(ERROR) << "setsockopt";
    return false;
  }
  return true;
}

TEST(Socket, Self) {
  ScopedFileHandle send_sock, recv_sock;
  ASSERT_TRUE(UnixSocketpair(&send_sock, &recv_sock));
  ASSERT_TRUE(SetSoPasscred(send_sock.get()));
  ASSERT_TRUE(SetSoPasscred(recv_sock.get()));

  char msg = 42;
  ASSERT_EQ(SendMsg(send_sock.get(), &msg, sizeof(msg)), 0);

  char recv_msg = 0;
  ucred creds;
  ASSERT_TRUE(RecvMsg(recv_sock.get(), &recv_msg, sizeof(recv_msg), &creds));
  EXPECT_EQ(recv_msg, msg);
  EXPECT_EQ(creds.pid, getpid());
  EXPECT_EQ(creds.uid, geteuid());
  EXPECT_EQ(creds.gid, getegid());
}

}  // namespace
}  // namespace test
}  // namespace crashpad
