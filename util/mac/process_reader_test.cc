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

#include "util/mac/process_reader.h"

#include <dispatch/dispatch.h>
#include <mach/mach.h>
#include <string.h>

#include <map>
#include <string>

#include "base/logging.h"
#include "base/mac/scoped_mach_port.h"
#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "util/file/fd_io.h"
#include "util/stdlib/pointer_container.h"
#include "util/test/mac/mach_errors.h"
#include "util/test/mac/mach_multiprocess.h"
#include "util/test/errors.h"

namespace {

using namespace crashpad;
using namespace crashpad::test;

TEST(ProcessReader, SelfBasic) {
  ProcessReader process_reader;
  ASSERT_TRUE(process_reader.Initialize(mach_task_self()));

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
      reinterpret_cast<mach_vm_address_t>(kTestMemory),
      sizeof(kTestMemory),
      &buffer));
  EXPECT_STREQ(kTestMemory, buffer);
}

const char kTestMemory[] = "Read me from another process";

class ProcessReaderChild final : public MachMultiprocess {
 public:
  ProcessReaderChild() : MachMultiprocess() {}

  ~ProcessReaderChild() {}

 private:
  void MachMultiprocessParent() override {
    ProcessReader process_reader;
    ASSERT_TRUE(process_reader.Initialize(ChildTask()));

#if !defined(ARCH_CPU_64_BITS)
    EXPECT_FALSE(process_reader.Is64Bit());
#else
    EXPECT_TRUE(process_reader.Is64Bit());
#endif

    EXPECT_EQ(getpid(), process_reader.ParentProcessID());
    EXPECT_EQ(ChildPID(), process_reader.ProcessID());

    int read_fd = ReadPipeFD();

    mach_vm_address_t address;
    int rv = ReadFD(read_fd, &address, sizeof(address));
    ASSERT_EQ(static_cast<ssize_t>(sizeof(address)), rv)
        << ErrnoMessage("read");

    std::string read_string;
    ASSERT_TRUE(process_reader.Memory()->ReadCString(address, &read_string));
    EXPECT_EQ(kTestMemory, read_string);

    // Tell the child that it’s OK to exit. The child needed to be kept alive
    // until the parent finished working with it.
    int write_fd = WritePipeFD();
    char c = '\0';
    rv = WriteFD(write_fd, &c, 1);
    ASSERT_EQ(1, rv) << ErrnoMessage("write");
  }

  void MachMultiprocessChild() override {
    int write_fd = WritePipeFD();

    mach_vm_address_t address =
        reinterpret_cast<mach_vm_address_t>(kTestMemory);
    int rv = WriteFD(write_fd, &address, sizeof(address));
    ASSERT_EQ(static_cast<ssize_t>(sizeof(address)), rv)
        << ErrnoMessage("write");

    // Wait for the parent to say that it’s OK to exit.
    int read_fd = ReadPipeFD();
    char c;
    rv = ReadFD(read_fd, &c, 1);
    ASSERT_EQ(1, rv) << ErrnoMessage("read");
  }

  DISALLOW_COPY_AND_ASSIGN(ProcessReaderChild);
};

TEST(ProcessReader, ChildBasic) {
  ProcessReaderChild process_reader_child;
  process_reader_child.Run();
}

// Returns a thread ID given a pthread_t. This wraps pthread_threadid_np() but
// that function has a cumbersome interface because it returns a success value.
// This function CHECKs success and returns the thread ID directly.
uint64_t PthreadToThreadID(pthread_t pthread) {
  uint64_t thread_id;
  int rv = pthread_threadid_np(pthread, &thread_id);
  CHECK_EQ(rv, 0);
  return thread_id;
}

TEST(ProcessReader, SelfOneThread) {
  ProcessReader process_reader;
  ASSERT_TRUE(process_reader.Initialize(mach_task_self()));

  const std::vector<ProcessReaderThread>& threads = process_reader.Threads();

  // If other tests ran in this process previously, threads may have been
  // created and may still be running. This check must look for at least one
  // thread, not exactly one thread.
  ASSERT_GE(threads.size(), 1u);

  EXPECT_EQ(PthreadToThreadID(pthread_self()), threads[0].id);

  base::mac::ScopedMachSendRight thread_self(mach_thread_self());
  EXPECT_EQ(thread_self, threads[0].port);

  EXPECT_EQ(0, threads[0].suspend_count);
}

class TestThreadPool {
 public:
  struct ThreadExpectation {
    mach_vm_address_t stack_address;
    int suspend_count;
  };

  TestThreadPool() : thread_infos_() {
  }

  // Resumes suspended threads, signals each thread’s exit semaphore asking it
  // to exit, and joins each thread, blocking until they have all exited.
  ~TestThreadPool() {
    for (ThreadInfo* thread_info : thread_infos_) {
      mach_port_t thread_port = pthread_mach_thread_np(thread_info->pthread);
      while (thread_info->suspend_count > 0) {
        kern_return_t kr = thread_resume(thread_port);
        EXPECT_EQ(KERN_SUCCESS, kr) << MachErrorMessage(kr, "thread_resume");
        --thread_info->suspend_count;
      }
    }

    for (const ThreadInfo* thread_info : thread_infos_) {
      dispatch_semaphore_signal(thread_info->exit_semaphore);
    }

    for (const ThreadInfo* thread_info : thread_infos_) {
      int rv = pthread_join(thread_info->pthread, NULL);
      CHECK_EQ(0, rv);
    }
  }

  // Starts |thread_count| threads and waits on each thread’s ready semaphore,
  // so that when this function returns, all threads have been started and have
  // all run to the point that they’ve signalled that they are ready.
  void StartThreads(size_t thread_count) {
    ASSERT_TRUE(thread_infos_.empty());

    for (size_t thread_index = 0; thread_index < thread_count; ++thread_index) {
      ThreadInfo* thread_info = new ThreadInfo();
      thread_infos_.push_back(thread_info);

      int rv = pthread_create(&thread_info->pthread,
                              NULL,
                              ThreadMain,
                              thread_info);
      ASSERT_EQ(0, rv);
    }

    for (const ThreadInfo* thread_info : thread_infos_) {
      long rv = dispatch_semaphore_wait(thread_info->ready_semaphore,
                                        DISPATCH_TIME_FOREVER);
      ASSERT_EQ(0, rv);
    }

    // If present, suspend the thread at indices 1 through 3 the same number of
    // times as their index. This tests reporting of suspend counts.
    for (size_t thread_index = 1;
         thread_index < thread_infos_.size() && thread_index < 4;
         ++thread_index) {
      mach_port_t thread_port =
          pthread_mach_thread_np(thread_infos_[thread_index]->pthread);
      for (size_t suspend_count = 0;
           suspend_count < thread_index;
           ++suspend_count) {
        kern_return_t kr = thread_suspend(thread_port);
        EXPECT_EQ(KERN_SUCCESS, kr) << MachErrorMessage(kr, "thread_suspend");
        if (kr == KERN_SUCCESS) {
          ++thread_infos_[thread_index]->suspend_count;
        }
      }
    }
  }

  uint64_t GetThreadInfo(size_t thread_index,
                         ThreadExpectation* expectation) {
    CHECK_LT(thread_index, thread_infos_.size());

    const ThreadInfo* thread_info = thread_infos_[thread_index];
    expectation->stack_address = thread_info->stack_address;
    expectation->suspend_count = thread_info->suspend_count;

    return PthreadToThreadID(thread_info->pthread);
  }

 private:
  struct ThreadInfo {
    ThreadInfo()
        : pthread(NULL),
          stack_address(0),
          ready_semaphore(dispatch_semaphore_create(0)),
          exit_semaphore(dispatch_semaphore_create(0)),
          suspend_count(0) {
    }

    ~ThreadInfo() {
      dispatch_release(exit_semaphore);
      dispatch_release(ready_semaphore);
    }

    // The thread’s ID, set at the time the thread is created.
    pthread_t pthread;

    // An address somewhere within the thread’s stack. The thread sets this in
    // its ThreadMain().
    mach_vm_address_t stack_address;

    // The worker thread signals ready_semaphore to indicate that it’s done
    // setting up its ThreadInfo structure. The main thread waits on this
    // semaphore before using any data that the worker thread is responsible for
    // setting.
    dispatch_semaphore_t ready_semaphore;

    // The worker thread waits on exit_semaphore to determine when it’s safe to
    // exit. The main thread signals exit_semaphore when it no longer needs the
    // worker thread.
    dispatch_semaphore_t exit_semaphore;

    // The thread’s suspend count.
    int suspend_count;
  };

  static void* ThreadMain(void* argument) {
    ThreadInfo* thread_info = static_cast<ThreadInfo*>(argument);

    thread_info->stack_address =
        reinterpret_cast<mach_vm_address_t>(&thread_info);

    dispatch_semaphore_signal(thread_info->ready_semaphore);
    dispatch_semaphore_wait(thread_info->exit_semaphore, DISPATCH_TIME_FOREVER);

    // Check this here after everything’s known to be synchronized, otherwise
    // there’s a race between the parent thread storing this thread’s pthread_t
    // in thread_info_pthread and this thread starting and attempting to access
    // it.
    CHECK_EQ(pthread_self(), thread_info->pthread);

    return NULL;
  }

  // This is a PointerVector because the address of a ThreadInfo object is
  // passed to each thread’s ThreadMain(), so they cannot move around in memory.
  PointerVector<ThreadInfo> thread_infos_;

  DISALLOW_COPY_AND_ASSIGN(TestThreadPool);
};

typedef std::map<uint64_t, TestThreadPool::ThreadExpectation> ThreadMap;

// Verifies that all of the threads in |threads|, obtained from ProcessReader,
// agree with the expectation in |thread_map|. If |tolerate_extra_threads| is
// true, |threads| is allowed to contain threads that are not listed in
// |thread_map|. This is useful when testing situations where code outside of
// the test’s control (such as system libraries) may start threads, or may have
// started threads prior to a test’s execution.
void ExpectSeveralThreads(ThreadMap* thread_map,
                          const std::vector<ProcessReaderThread>& threads,
                          const bool tolerate_extra_threads) {
  if (tolerate_extra_threads) {
    ASSERT_GE(threads.size(), thread_map->size());
  } else {
    ASSERT_EQ(thread_map->size(), threads.size());
  }

  for (size_t thread_index = 0; thread_index < threads.size(); ++thread_index) {
    const ProcessReaderThread& thread = threads[thread_index];
    mach_vm_address_t thread_stack_region_end =
        thread.stack_region_address + thread.stack_region_size;

    const auto& iterator = thread_map->find(thread.id);
    if (!tolerate_extra_threads) {
      // Make sure that the thread is in the expectation map.
      ASSERT_NE(thread_map->end(), iterator);
    }

    if (iterator != thread_map->end()) {
      EXPECT_GE(iterator->second.stack_address, thread.stack_region_address);
      EXPECT_LT(iterator->second.stack_address, thread_stack_region_end);

      EXPECT_EQ(iterator->second.suspend_count, thread.suspend_count);

      // Remove the thread from the expectation map since it’s already been
      // found. This makes it easy to check for duplicate thread IDs, and makes
      // it easy to check that all expected threads were found.
      thread_map->erase(iterator);
    }

    // Make sure that this thread’s ID, stack region, and port don’t conflict
    // with any other thread’s. Each thread should have a unique value for its
    // ID and port, and each should have its own stack that doesn’t touch any
    // other thread’s stack.
    for (size_t other_thread_index = 0;
         other_thread_index < threads.size();
         ++other_thread_index) {
      if (thread_index == other_thread_index) {
        continue;
      }

      const ProcessReaderThread& other_thread = threads[other_thread_index];

      EXPECT_NE(thread.id, other_thread.id);
      EXPECT_NE(thread.port, other_thread.port);

      mach_vm_address_t other_thread_stack_region_end =
          other_thread.stack_region_address + other_thread.stack_region_size;
      EXPECT_FALSE(
          thread.stack_region_address >= other_thread.stack_region_address &&
          thread.stack_region_address < other_thread_stack_region_end);
      EXPECT_FALSE(
          thread_stack_region_end > other_thread.stack_region_address &&
          thread_stack_region_end <= other_thread_stack_region_end);
    }
  }

  // Make sure that each expected thread was found.
  EXPECT_TRUE(thread_map->empty());
}

TEST(ProcessReader, SelfSeveralThreads) {
  // Set up the ProcessReader here, before any other threads are running. This
  // tests that the threads it returns are lazily initialized as a snapshot of
  // the threads at the time of the first call to Threads(), and not at the
  // time the ProcessReader was created or initialized.
  ProcessReader process_reader;
  ASSERT_TRUE(process_reader.Initialize(mach_task_self()));

  TestThreadPool thread_pool;
  const size_t kChildThreads = 16;
  thread_pool.StartThreads(kChildThreads);
  if (Test::HasFatalFailure()) {
    return;
  }

  // Build a map of all expected threads, keyed by each thread’s ID. The values
  // are addresses that should lie somewhere within each thread’s stack.
  ThreadMap thread_map;
  const uint64_t self_thread_id = PthreadToThreadID(pthread_self());
  TestThreadPool::ThreadExpectation expectation;
  expectation.stack_address = reinterpret_cast<mach_vm_address_t>(&thread_map);
  expectation.suspend_count = 0;
  thread_map[self_thread_id] = expectation;
  for (size_t thread_index = 0; thread_index < kChildThreads; ++thread_index) {
    uint64_t thread_id = thread_pool.GetThreadInfo(thread_index, &expectation);

    // There can’t be any duplicate thread IDs.
    EXPECT_EQ(0u, thread_map.count(thread_id));

    thread_map[thread_id] = expectation;
  }

  const std::vector<ProcessReaderThread>& threads = process_reader.Threads();

  // Other tests that have run previously may have resulted in the creation of
  // threads that still exist, so pass true for |tolerate_extra_threads|.
  ExpectSeveralThreads(&thread_map, threads, true);

  // When testing in-process, verify that when this thread shows up in the
  // vector, it has the expected thread port, and that this thread port only
  // shows up once.
  base::mac::ScopedMachSendRight thread_self(mach_thread_self());
  bool found_thread_self = false;
  for (const ProcessReaderThread& thread : threads) {
    if (thread.port == thread_self) {
      EXPECT_FALSE(found_thread_self);
      found_thread_self = true;
      EXPECT_EQ(self_thread_id, thread.id);
    }
  }
  EXPECT_TRUE(found_thread_self);
}

class ProcessReaderThreadedChild final : public MachMultiprocess {
 public:
  explicit ProcessReaderThreadedChild(size_t thread_count)
      : MachMultiprocess(),
        thread_count_(thread_count) {
  }

  ~ProcessReaderThreadedChild() {}

 private:
  void MachMultiprocessParent() override {
    ProcessReader process_reader;
    ASSERT_TRUE(process_reader.Initialize(ChildTask()));

    int read_fd = ReadPipeFD();

    // Build a map of all expected threads, keyed by each thread’s ID, and with
    // addresses that should lie somewhere within each thread’s stack as values.
    // These IDs and addresses all come from the child process via the pipe.
    ThreadMap thread_map;
    for (size_t thread_index = 0;
         thread_index < thread_count_ + 1;
         ++thread_index) {
      uint64_t thread_id;
      int rv = ReadFD(read_fd, &thread_id, sizeof(thread_id));
      ASSERT_EQ(static_cast<ssize_t>(sizeof(thread_id)), rv)
          << ErrnoMessage("read");

      TestThreadPool::ThreadExpectation expectation;
      rv = ReadFD(read_fd,
                  &expectation.stack_address,
                  sizeof(expectation.stack_address));
      ASSERT_EQ(static_cast<ssize_t>(sizeof(expectation.stack_address)), rv)
          << ErrnoMessage("read");

      rv = ReadFD(read_fd,
                  &expectation.suspend_count,
                  sizeof(expectation.suspend_count));
      ASSERT_EQ(static_cast<ssize_t>(sizeof(expectation.suspend_count)), rv)
          << ErrnoMessage("read");

      // There can’t be any duplicate thread IDs.
      EXPECT_EQ(0u, thread_map.count(thread_id));

      thread_map[thread_id] = expectation;
    }

    const std::vector<ProcessReaderThread>& threads = process_reader.Threads();

    // The child shouldn’t have any threads other than its main thread and the
    // ones it created in its pool, so pass false for |tolerate_extra_threads|.
    ExpectSeveralThreads(&thread_map, threads, false);

    // Tell the child that it’s OK to exit. The child needed to be kept alive
    // until the parent finished working with it.
    int write_fd = WritePipeFD();
    char c = '\0';
    int rv = WriteFD(write_fd, &c, 1);
    ASSERT_EQ(1, rv) << ErrnoMessage("write");
  }

  void MachMultiprocessChild() override {
    TestThreadPool thread_pool;
    thread_pool.StartThreads(thread_count_);
    if (testing::Test::HasFatalFailure()) {
      return;
    }

    int write_fd = WritePipeFD();

    // This thread isn’t part of the thread pool, but the parent will be able
    // to inspect it. Write an entry for it.
    uint64_t thread_id = PthreadToThreadID(pthread_self());

    int rv = WriteFD(write_fd, &thread_id, sizeof(thread_id));
    ASSERT_EQ(static_cast<ssize_t>(sizeof(thread_id)), rv)
        << ErrnoMessage("write");

    TestThreadPool::ThreadExpectation expectation;
    expectation.stack_address = reinterpret_cast<mach_vm_address_t>(&thread_id);
    expectation.suspend_count = 0;

    rv = WriteFD(write_fd,
                 &expectation.stack_address,
                 sizeof(expectation.stack_address));
    ASSERT_EQ(static_cast<ssize_t>(sizeof(expectation.stack_address)), rv)
        << ErrnoMessage("write");

    rv = WriteFD(write_fd,
                 &expectation.suspend_count,
                 sizeof(expectation.suspend_count));
    ASSERT_EQ(static_cast<ssize_t>(sizeof(expectation.suspend_count)), rv)
        << ErrnoMessage("write");

    // Write an entry for everything in the thread pool.
    for (size_t thread_index = 0;
         thread_index < thread_count_;
         ++thread_index) {
      uint64_t thread_id =
          thread_pool.GetThreadInfo(thread_index, &expectation);

      rv = WriteFD(write_fd, &thread_id, sizeof(thread_id));
      ASSERT_EQ(static_cast<ssize_t>(sizeof(thread_id)), rv)
          << ErrnoMessage("write");

      rv = WriteFD(write_fd,
                   &expectation.stack_address,
                   sizeof(expectation.stack_address));
      ASSERT_EQ(static_cast<ssize_t>(sizeof(expectation.stack_address)), rv)
          << ErrnoMessage("write");

      rv = WriteFD(write_fd,
                   &expectation.suspend_count,
                   sizeof(expectation.suspend_count));
      ASSERT_EQ(static_cast<ssize_t>(sizeof(expectation.suspend_count)), rv)
          << ErrnoMessage("write");
    }

    // Wait for the parent to say that it’s OK to exit.
    int read_fd = ReadPipeFD();
    char c;
    rv = ReadFD(read_fd, &c, 1);
    ASSERT_EQ(1, rv) << ErrnoMessage("read");
  }

  size_t thread_count_;

  DISALLOW_COPY_AND_ASSIGN(ProcessReaderThreadedChild);
};

TEST(ProcessReader, ChildOneThread) {
  // The main thread plus zero child threads equals one thread.
  const size_t kChildThreads = 0;
  ProcessReaderThreadedChild process_reader_threaded_child(kChildThreads);
  process_reader_threaded_child.Run();
}

TEST(ProcessReader, ChildSeveralThreads) {
  const size_t kChildThreads = 64;
  ProcessReaderThreadedChild process_reader_threaded_child(kChildThreads);
  process_reader_threaded_child.Run();
}

}  // namespace
