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

#include "util/linux/ptrace_broker.h"

#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>

#include "build/build_config.h"
#include "gtest/gtest.h"
#include "test/multiprocess.h"
#include "util/file/file_io.h"
#include "util/linux/ptrace_client.h"
#include "util/synchronization/semaphore.h"
#include "util/thread/thread.h"

namespace crashpad {
namespace test {
namespace {

class ScopedTimeoutThread : public Thread {
 public:
  ScopedTimeoutThread() : join_sem_(0) {}
  ~ScopedTimeoutThread() { EXPECT_TRUE(JoinWithTimeout(5.0)); }

 protected:
  void ThreadMain() override { join_sem_.Signal(); }

 private:
  bool JoinWithTimeout(double timeout) {
    if (!join_sem_.TimedWait(timeout)) {
      return false;
    }
    Join();
    return true;
  }

  Semaphore join_sem_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTimeoutThread);
};

class RunBrokerThread : public ScopedTimeoutThread {
 public:
  RunBrokerThread(PtraceBroker* broker)
      : ScopedTimeoutThread(), broker_(broker) {}

  ~RunBrokerThread() {}

 private:
  void ThreadMain() override {
    EXPECT_TRUE(broker_->Run());
    ScopedTimeoutThread::ThreadMain();
  }

  PtraceBroker* broker_;

  DISALLOW_COPY_AND_ASSIGN(RunBrokerThread);
};

class BlockOnReadThread : public ScopedTimeoutThread {
 public:
  BlockOnReadThread(int readfd, int writefd)
      : ScopedTimeoutThread(), readfd_(readfd), writefd_(writefd) {}

  ~BlockOnReadThread() {}

 private:
  void ThreadMain() override {
    pid_t pid = syscall(SYS_gettid);
    LoggingWriteFile(writefd_, &pid, sizeof(pid));
    CheckedReadFileAtEOF(readfd_);
    ScopedTimeoutThread::ThreadMain();
  }

  int readfd_;
  int writefd_;

  DISALLOW_COPY_AND_ASSIGN(BlockOnReadThread);
};

class SameBitnessTest : public Multiprocess {
 public:
  SameBitnessTest() : Multiprocess() {}
  ~SameBitnessTest() {}

 private:
  void MultiprocessParent() override {
    pid_t child_tid;
    LoggingReadFileExactly(ReadPipeHandle(), &child_tid, sizeof(child_tid));

    int socks[2];
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, socks), 0);
    ScopedFileHandle broker_sock(socks[0]);
    ScopedFileHandle client_sock(socks[1]);

#if defined(ARCH_CPU_64_BITS)
    constexpr bool am_64_bit = true;
#else
    constexpr bool am_64_bit = false;
#endif  // ARCH_CPU_64_BITS
    PtraceBroker broker(broker_sock.get(), am_64_bit);
    RunBrokerThread broker_thread(&broker);
    broker_thread.Start();

    {
      PtraceClient client;
      ASSERT_TRUE(client.Initialize(client_sock.get(), ChildPID()));

      EXPECT_EQ(client.GetProcessID(), ChildPID());
      EXPECT_TRUE(client.Attach(child_tid));
      EXPECT_EQ(client.Is64Bit(), am_64_bit);

      ThreadInfo info1;
      ASSERT_TRUE(client.GetThreadInfo(ChildPID(), &info1));
      ThreadInfo info2;
      ASSERT_TRUE(client.GetThreadInfo(child_tid, &info2));
    }
  }

  void MultiprocessChild() override {
    BlockOnReadThread thread(ReadPipeHandle(), WritePipeHandle());
    thread.Start();

    CheckedReadFileAtEOF(ReadPipeHandle());
  }

  DISALLOW_COPY_AND_ASSIGN(SameBitnessTest);
};

TEST(PtraceBroker, SameBitness) {
  SameBitnessTest test;
  test.Run();
}

// TODO(jperaza): Test against a process with different bitness.

}  // namespace
}  // namespace test
}  // namespace crashpad
