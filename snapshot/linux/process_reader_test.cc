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

#include "snapshot/linux/process_reader.h"

#include <pthread.h>

#include "build/build_config.h"
#include "gtest/gtest.h"
#include "test/errors.h"
#include "test/multiprocess.h"
#include "util/file/file_io.h"
#include "util/stdlib/pointer_container.h"
#include "util/synchronization/semaphore.h"

namespace crashpad {
namespace test {
namespace {

TEST(ProcessReader, SelfBasic) {
  ProcessReader process_reader;
  ASSERT_TRUE(process_reader.Initialize(getpid()));

#if !defined(ARCH_CPU_64_BITS)
  EXPECT_FALSE(process_reader.Is64Bit());
#else
  EXPECT_TRUE(process_reader.Is64Bit());
#endif

  EXPECT_EQ(getpid(), process_reader.ProcessID());
  EXPECT_EQ(getppid(), process_reader.ParentProcessID());

  const char kTestMemory[] = "Some test memory";
  char buffer[arraysize(kTestMemory)];
  ASSERT_TRUE(process_reader.Memory()->Read(
      reinterpret_cast<LinuxVMAddress>(kTestMemory),
      sizeof(kTestMemory),
      &buffer));
  EXPECT_STREQ(kTestMemory, buffer);
}

const char kTestMemory[] = "Read me from another process";

class BasicChildTest : public Multiprocess {
 public:
  BasicChildTest() : Multiprocess() {}
  ~BasicChildTest() {}

 private:
  void MultiprocessParent() override {
    ProcessReader process_reader;
    ASSERT_TRUE(process_reader.Initialize(ChildPID()));

#if !defined(ARCH_CPU_64_BITS)
    EXPECT_FALSE(process_reader.Is64Bit());
#else
    EXPECT_TRUE(process_reader.Is64Bit());
#endif

    EXPECT_EQ(process_reader.ParentProcessID(), getpid());
    EXPECT_EQ(process_reader.ProcessID(), ChildPID());

    std::string read_string;
    ASSERT_TRUE(process_reader.Memory()->ReadCString(
        reinterpret_cast<LinuxVMAddress>(kTestMemory), &read_string));
    EXPECT_EQ(read_string, kTestMemory);
  }

  void MultiprocessChild() override { CheckedReadFileAtEOF(ReadPipeHandle()); }

  DISALLOW_COPY_AND_ASSIGN(BasicChildTest);
};

TEST(ProcessReader, ChildBasic) {
  BasicChildTest test;
  test.Run();
}

TEST(ProcessReader, SelfOneThread) {
  ProcessReader process_reader;
  ASSERT_TRUE(process_reader.Initialize(getpid()));

  const std::vector<ProcessReader::Thread>& threads = process_reader.Threads();

  ASSERT_GE(threads.size(), 1u);
  EXPECT_EQ(threads[0].tid, getpid());
}

class TestThreadPool {
 public:
  TestThreadPool() : thread_infos_() {}

  ~TestThreadPool() {
    for (ThreadInfo* thread_info : thread_infos_) {
      thread_info->exit_semaphore.Signal();
    }

    for (const ThreadInfo* thread_info : thread_infos_) {
      int rv = pthread_join(thread_info->pthread, nullptr);
      CHECK_EQ(rv, 0);
    }
  }

  void StartThreads(size_t thread_count) {
    ASSERT_TRUE(thread_infos_.empty());

    for (size_t thread_index = 0; thread_index < thread_count; ++thread_index) {
      ThreadInfo* thread_info = new ThreadInfo();
      thread_infos_.push_back(thread_info);

      int rv = pthread_create(&thread_info->pthread,
                              nullptr,
                              ThreadMain,
                              thread_info);
      ASSERT_EQ(rv, 0);
    }

    for (ThreadInfo* thread_info : thread_infos_) {
      thread_info->ready_semaphore.Wait();
    }

  }
 private:
  struct ThreadInfo {
    ThreadInfo()
        : pthread(),
          ready_semaphore(0),
          exit_semaphore(0) {}

    ~ThreadInfo() {}

    pthread_t pthread;
    Semaphore ready_semaphore;
    Semaphore exit_semaphore;
  };

  static void* ThreadMain(void* argument) {
    ThreadInfo* thread_info = static_cast<ThreadInfo*>(argument);

    thread_info->ready_semaphore.Signal();
    thread_info->exit_semaphore.Wait();

    CHECK_EQ(pthread_self(), thread_info->pthread);

    return nullptr;
  }

  PointerVector<ThreadInfo> thread_infos_;

  DISALLOW_COPY_AND_ASSIGN(TestThreadPool);
};

TEST(ProcessReader, SelfSeveralThreads) {
  ProcessReader process_reader;
  ASSERT_TRUE(process_reader.Initialize(getpid()));

  TestThreadPool thread_pool;
  const size_t kChildThreads = 16;
  ASSERT_NO_FATAL_FAILURE(thread_pool.StartThreads(kChildThreads));

  const std::vector<ProcessReader::Thread>& threads = process_reader.Threads();

  ASSERT_GE(threads.size(), kChildThreads);
  EXPECT_EQ(threads[0].tid, getpid());
  for (size_t thread_index = 0; thread_index < threads.size(); ++thread_index) {
    for (size_t other_thread_index = thread_index + 1;
         other_thread_index < threads.size();
         ++other_thread_index) {
      EXPECT_NE(threads[thread_index].tid, threads[other_thread_index].tid);
    }
  }
}

}  // namespace
}  // namespace test
}  // namespace crashpad
