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

#include <sys/types.h>
#include <sys/socket.h>

#include "build/build_config.h"
#include "gtest/gtest.h"
#include "test/multiprocess.h"
#include "util/file/file_io.h"
#include "util/linux/ptrace_client.h"
#include "util/misc/from_pointer_cast.h"
#include "util/synchronization/semaphore.h"
#include "util/thread/thread.h"

namespace crashpad {
namespace test {
namespace {

class RunBrokerThread : public Thread {
  public:
   RunBrokerThread(PtraceBroker* broker) : Thread(), broker_(broker), join_sem_(0) {}
   ~RunBrokerThread() {}

   bool JoinWithTimeout(double timeout) {
     if (!join_sem_.TimedWait(timeout)) {
       return false;
     }
     Join();
     return true;
   }

  private:
   // Thread:
   void ThreadMain() override {
     EXPECT_TRUE(broker_->Run());
     join_sem_.Signal();
   }

   PtraceBroker* broker_;
   Semaphore join_sem_;

   DISALLOW_COPY_AND_ASSIGN(RunBrokerThread);
};

class SameBitnessTest : public Multiprocess {
 public:
  SameBitnessTest() : Multiprocess() {}
  ~SameBitnessTest() {}

 private:
  void MultiprocessParent() override {
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
    RunBrokerThread broker_thread(&broker); // TODO scoping
    broker_thread.Start();

    PtraceClient client;
    ASSERT_TRUE(client.Initialize(client_sock.get(), ChildPID()));
  }

  void MultiprocessChild() override {
    CheckedReadFileAtEOF(ReadPipeHandle());
  }

  DISALLOW_COPY_AND_ASSIGN(SameBitnessTest);
};

TEST(Ptracer, SameBitness) {
  SameBitnessTest test;
  test.Run();
}

// TODO(jperaza): Test against a process with different bitness.

}  // namespace
}  // namespace test
}  // namespace crashpad
