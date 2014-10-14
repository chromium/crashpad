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

#include <mach-o/dyld.h>
#include <mach-o/dyld_images.h>
#include <mach/mach.h>
#include <string.h>
#include <sys/stat.h>

#include <map>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/mac/scoped_mach_port.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "util/file/fd_io.h"
#include "util/mac/mach_o_image_reader.h"
#include "util/mach/mach_extensions.h"
#include "util/stdlib/pointer_container.h"
#include "util/synchronization/semaphore.h"
#include "util/test/errors.h"
#include "util/test/mac/dyld.h"
#include "util/test/mac/mach_errors.h"
#include "util/test/mac/mach_multiprocess.h"

namespace crashpad {
namespace test {
namespace {

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
    CheckedReadFD(read_fd, &address, sizeof(address));

    std::string read_string;
    ASSERT_TRUE(process_reader.Memory()->ReadCString(address, &read_string));
    EXPECT_EQ(kTestMemory, read_string);
  }

  void MachMultiprocessChild() override {
    int write_fd = WritePipeFD();

    mach_vm_address_t address =
        reinterpret_cast<mach_vm_address_t>(kTestMemory);
    CheckedWriteFD(write_fd, &address, sizeof(address));

    // Wait for the parent to signal that it’s OK to exit by closing its end of
    // the pipe.
    CheckedReadFDAtEOF(ReadPipeFD());
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

  const std::vector<ProcessReader::Thread>& threads = process_reader.Threads();

  // If other tests ran in this process previously, threads may have been
  // created and may still be running. This check must look for at least one
  // thread, not exactly one thread.
  ASSERT_GE(threads.size(), 1u);

  EXPECT_EQ(PthreadToThreadID(pthread_self()), threads[0].id);

  thread_t thread_self = MachThreadSelf();
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
      thread_t thread_port = pthread_mach_thread_np(thread_info->pthread);
      while (thread_info->suspend_count > 0) {
        kern_return_t kr = thread_resume(thread_port);
        EXPECT_EQ(KERN_SUCCESS, kr) << MachErrorMessage(kr, "thread_resume");
        --thread_info->suspend_count;
      }
    }

    for (ThreadInfo* thread_info : thread_infos_) {
      thread_info->exit_semaphore.Signal();
    }

    for (const ThreadInfo* thread_info : thread_infos_) {
      int rv = pthread_join(thread_info->pthread, nullptr);
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
                              nullptr,
                              ThreadMain,
                              thread_info);
      ASSERT_EQ(0, rv);
    }

    for (ThreadInfo* thread_info : thread_infos_) {
      thread_info->ready_semaphore.Wait();
    }

    // If present, suspend the thread at indices 1 through 3 the same number of
    // times as their index. This tests reporting of suspend counts.
    for (size_t thread_index = 1;
         thread_index < thread_infos_.size() && thread_index < 4;
         ++thread_index) {
      thread_t thread_port =
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
        : pthread(nullptr),
          stack_address(0),
          ready_semaphore(0),
          exit_semaphore(0),
          suspend_count(0) {
    }

    ~ThreadInfo() {}

    // The thread’s ID, set at the time the thread is created.
    pthread_t pthread;

    // An address somewhere within the thread’s stack. The thread sets this in
    // its ThreadMain().
    mach_vm_address_t stack_address;

    // The worker thread signals ready_semaphore to indicate that it’s done
    // setting up its ThreadInfo structure. The main thread waits on this
    // semaphore before using any data that the worker thread is responsible for
    // setting.
    Semaphore ready_semaphore;

    // The worker thread waits on exit_semaphore to determine when it’s safe to
    // exit. The main thread signals exit_semaphore when it no longer needs the
    // worker thread.
    Semaphore exit_semaphore;

    // The thread’s suspend count.
    int suspend_count;
  };

  static void* ThreadMain(void* argument) {
    ThreadInfo* thread_info = static_cast<ThreadInfo*>(argument);

    thread_info->stack_address =
        reinterpret_cast<mach_vm_address_t>(&thread_info);

    thread_info->ready_semaphore.Signal();
    thread_info->exit_semaphore.Wait();

    // Check this here after everything’s known to be synchronized, otherwise
    // there’s a race between the parent thread storing this thread’s pthread_t
    // in thread_info_pthread and this thread starting and attempting to access
    // it.
    CHECK_EQ(pthread_self(), thread_info->pthread);

    return nullptr;
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
                          const std::vector<ProcessReader::Thread>& threads,
                          const bool tolerate_extra_threads) {
  if (tolerate_extra_threads) {
    ASSERT_GE(threads.size(), thread_map->size());
  } else {
    ASSERT_EQ(thread_map->size(), threads.size());
  }

  for (size_t thread_index = 0; thread_index < threads.size(); ++thread_index) {
    const ProcessReader::Thread& thread = threads[thread_index];
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

      const ProcessReader::Thread& other_thread = threads[other_thread_index];

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
  ASSERT_NO_FATAL_FAILURE(thread_pool.StartThreads(kChildThreads));

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

  const std::vector<ProcessReader::Thread>& threads = process_reader.Threads();

  // Other tests that have run previously may have resulted in the creation of
  // threads that still exist, so pass true for |tolerate_extra_threads|.
  ExpectSeveralThreads(&thread_map, threads, true);

  // When testing in-process, verify that when this thread shows up in the
  // vector, it has the expected thread port, and that this thread port only
  // shows up once.
  thread_t thread_self = MachThreadSelf();
  bool found_thread_self = false;
  for (const ProcessReader::Thread& thread : threads) {
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
      CheckedReadFD(read_fd, &thread_id, sizeof(thread_id));

      TestThreadPool::ThreadExpectation expectation;
      CheckedReadFD(read_fd,
                    &expectation.stack_address,
                    sizeof(expectation.stack_address));
      CheckedReadFD(read_fd,
                    &expectation.suspend_count,
                    sizeof(expectation.suspend_count));

      // There can’t be any duplicate thread IDs.
      EXPECT_EQ(0u, thread_map.count(thread_id));

      thread_map[thread_id] = expectation;
    }

    const std::vector<ProcessReader::Thread>& threads = process_reader.Threads();

    // The child shouldn’t have any threads other than its main thread and the
    // ones it created in its pool, so pass false for |tolerate_extra_threads|.
    ExpectSeveralThreads(&thread_map, threads, false);
  }

  void MachMultiprocessChild() override {
    TestThreadPool thread_pool;
    ASSERT_NO_FATAL_FAILURE(thread_pool.StartThreads(thread_count_));

    int write_fd = WritePipeFD();

    // This thread isn’t part of the thread pool, but the parent will be able
    // to inspect it. Write an entry for it.
    uint64_t thread_id = PthreadToThreadID(pthread_self());

    CheckedWriteFD(write_fd, &thread_id, sizeof(thread_id));

    TestThreadPool::ThreadExpectation expectation;
    expectation.stack_address = reinterpret_cast<mach_vm_address_t>(&thread_id);
    expectation.suspend_count = 0;

    CheckedWriteFD(write_fd,
                   &expectation.stack_address,
                   sizeof(expectation.stack_address));
    CheckedWriteFD(write_fd,
                   &expectation.suspend_count,
                   sizeof(expectation.suspend_count));

    // Write an entry for everything in the thread pool.
    for (size_t thread_index = 0;
         thread_index < thread_count_;
         ++thread_index) {
      uint64_t thread_id =
          thread_pool.GetThreadInfo(thread_index, &expectation);

      CheckedWriteFD(write_fd, &thread_id, sizeof(thread_id));
      CheckedWriteFD(write_fd,
                     &expectation.stack_address,
                     sizeof(expectation.stack_address));
      CheckedWriteFD(write_fd,
                     &expectation.suspend_count,
                     sizeof(expectation.suspend_count));
    }

    // Wait for the parent to signal that it’s OK to exit by closing its end of
    // the pipe.
    CheckedReadFDAtEOF(ReadPipeFD());
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

TEST(ProcessReader, SelfModules) {
  ProcessReader process_reader;
  ASSERT_TRUE(process_reader.Initialize(mach_task_self()));

  uint32_t dyld_image_count = _dyld_image_count();
  const std::vector<ProcessReader::Module>& modules = process_reader.Modules();

  // There needs to be at least an entry for the main executable, for a dylib,
  // and for dyld.
  ASSERT_GE(modules.size(), 3u);

  // dyld_image_count doesn’t include an entry for dyld itself, but |modules|
  // does.
  ASSERT_EQ(dyld_image_count + 1, modules.size());

  for (uint32_t index = 0; index < dyld_image_count; ++index) {
    SCOPED_TRACE(base::StringPrintf(
        "index %u, name %s", index, modules[index].name.c_str()));

    const char* dyld_image_name = _dyld_get_image_name(index);
    EXPECT_EQ(dyld_image_name, modules[index].name);
    EXPECT_EQ(
        reinterpret_cast<mach_vm_address_t>(_dyld_get_image_header(index)),
        modules[index].reader->Address());

    if (index == 0) {
      // dyld didn’t load the main executable, so it couldn’t record its
      // timestamp, and it is reported as 0.
      EXPECT_EQ(0, modules[index].timestamp);
    } else {
      // Hope that the module didn’t change on disk.
      struct stat stat_buf;
      int rv = stat(dyld_image_name, &stat_buf);
      EXPECT_EQ(0, rv) << ErrnoMessage("stat");
      if (rv == 0) {
        EXPECT_EQ(stat_buf.st_mtime, modules[index].timestamp);
      }
    }
  }

  size_t index = modules.size() - 1;
  EXPECT_EQ("/usr/lib/dyld", modules[index].name);

  // dyld didn’t load itself either, so it couldn’t record its timestamp, and it
  // is also reported as 0.
  EXPECT_EQ(0, modules[index].timestamp);

  const struct dyld_all_image_infos* dyld_image_infos =
      _dyld_get_all_image_infos();
  if (dyld_image_infos->version >= 2) {
    EXPECT_EQ(
        reinterpret_cast<mach_vm_address_t>(
            dyld_image_infos->dyldImageLoadAddress),
        modules[index].reader->Address());
  }
}

class ProcessReaderModulesChild final : public MachMultiprocess {
 public:
  ProcessReaderModulesChild() : MachMultiprocess() {}

  ~ProcessReaderModulesChild() {}

 private:
  void MachMultiprocessParent() override {
    ProcessReader process_reader;
    ASSERT_TRUE(process_reader.Initialize(ChildTask()));

    const std::vector<ProcessReader::Module>& modules =
        process_reader.Modules();

    // There needs to be at least an entry for the main executable, for a dylib,
    // and for dyld.
    ASSERT_GE(modules.size(), 3u);

    int read_fd = ReadPipeFD();

    uint32_t expect_modules;
    CheckedReadFD(read_fd, &expect_modules, sizeof(expect_modules));

    ASSERT_EQ(expect_modules, modules.size());

    for (size_t index = 0; index < modules.size(); ++index) {
      SCOPED_TRACE(base::StringPrintf(
          "index %zu, name %s", index, modules[index].name.c_str()));

      uint32_t expect_name_length;
      CheckedReadFD(
          read_fd, &expect_name_length, sizeof(expect_name_length));

      // The NUL terminator is not read.
      std::string expect_name(expect_name_length, '\0');
      CheckedReadFD(read_fd, &expect_name[0], expect_name_length);
      EXPECT_EQ(expect_name, modules[index].name);

      mach_vm_address_t expect_address;
      CheckedReadFD(read_fd, &expect_address, sizeof(expect_address));
      EXPECT_EQ(expect_address, modules[index].reader->Address());

      if (index == 0 || index == modules.size() - 1) {
        // dyld didn’t load the main executable or itself, so it couldn’t record
        // these timestamps, and they are reported as 0.
        EXPECT_EQ(0, modules[index].timestamp);
      } else {
        // Hope that the module didn’t change on disk.
        struct stat stat_buf;
        int rv = stat(expect_name.c_str(), &stat_buf);
        EXPECT_EQ(0, rv) << ErrnoMessage("stat");
        if (rv == 0) {
          EXPECT_EQ(stat_buf.st_mtime, modules[index].timestamp);
        }
      }
    }
  }

  void MachMultiprocessChild() override {
    int write_fd = WritePipeFD();

    uint32_t dyld_image_count = _dyld_image_count();
    const struct dyld_all_image_infos* dyld_image_infos =
        _dyld_get_all_image_infos();

    uint32_t write_image_count = dyld_image_count;
    if (dyld_image_infos->version >= 2) {
      // dyld_image_count doesn’t include an entry for dyld itself, but one will
      // be written.
      ++write_image_count;
    }

    CheckedWriteFD(write_fd, &write_image_count, sizeof(write_image_count));

    for (size_t index = 0; index < write_image_count; ++index) {
      const char* dyld_image_name;
      mach_vm_address_t dyld_image_address;

      if (index < dyld_image_count) {
        dyld_image_name = _dyld_get_image_name(index);
        dyld_image_address =
            reinterpret_cast<mach_vm_address_t>(_dyld_get_image_header(index));
      } else {
        dyld_image_name = "/usr/lib/dyld";
        dyld_image_address = reinterpret_cast<mach_vm_address_t>(
            dyld_image_infos->dyldImageLoadAddress);
      }

      uint32_t dyld_image_name_length = strlen(dyld_image_name);
      CheckedWriteFD(
          write_fd, &dyld_image_name_length, sizeof(dyld_image_name_length));

      // The NUL terminator is not written.
      CheckedWriteFD(write_fd, dyld_image_name, dyld_image_name_length);

      CheckedWriteFD(write_fd, &dyld_image_address, sizeof(dyld_image_address));
    }

    // Wait for the parent to signal that it’s OK to exit by closing its end of
    // the pipe.
    CheckedReadFDAtEOF(ReadPipeFD());
  }

  DISALLOW_COPY_AND_ASSIGN(ProcessReaderModulesChild);
};

TEST(ProcessReader, ChildModules) {
  ProcessReaderModulesChild process_reader_modules_child;
  process_reader_modules_child.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
