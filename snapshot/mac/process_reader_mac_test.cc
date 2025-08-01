// Copyright 2014 The Crashpad Authors
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

#include "snapshot/mac/process_reader_mac.h"

#include <Availability.h>
#include <OpenCL/opencl.h>
#include <dlfcn.h>
#include <errno.h>
#include <mach-o/dyld.h>
#include <mach-o/dyld_images.h>
#include <mach-o/loader.h>
#include <mach/mach.h>
#include <pthread.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <iterator>
#include <map>
#include <unordered_set>
#include <utility>

#include "base/apple/mach_logging.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "snapshot/mac/mach_o_image_reader.h"
#include "snapshot/mac/mach_o_image_segment_reader.h"
#include "test/errors.h"
#include "test/mac/dyld.h"
#include "test/mac/mach_errors.h"
#include "test/mac/mach_multiprocess.h"
#include "test/scoped_set_thread_name.h"
#include "util/file/file_io.h"
#include "util/mac/mac_util.h"
#include "util/mach/mach_extensions.h"
#include "util/misc/from_pointer_cast.h"
#include "util/synchronization/semaphore.h"

namespace crashpad {
namespace test {
namespace {

#if !defined(ARCH_CPU_64_BITS)
using MachHeader = mach_header;
#else
using MachHeader = mach_header_64;
#endif

using ModulePathAndAddress = std::pair<std::string, mach_vm_address_t>;
struct PathAndAddressHash {
  std::size_t operator()(const ModulePathAndAddress& pair) const {
    return std::hash<std::string>()(pair.first) ^
           std::hash<mach_vm_address_t>()(pair.second);
  }
};
using ModuleSet = std::unordered_set<ModulePathAndAddress, PathAndAddressHash>;

constexpr char kDyldPath[] = "/usr/lib/dyld";

TEST(ProcessReaderMac, SelfBasic) {
  ProcessReaderMac process_reader;
  ASSERT_TRUE(process_reader.Initialize(mach_task_self()));

#if !defined(ARCH_CPU_64_BITS)
  EXPECT_FALSE(process_reader.Is64Bit());
#else
  EXPECT_TRUE(process_reader.Is64Bit());
#endif

  EXPECT_EQ(process_reader.ProcessID(), getpid());
  EXPECT_EQ(process_reader.ParentProcessID(), getppid());

  static constexpr char kTestMemory[] = "Some test memory";
  char buffer[std::size(kTestMemory)];
  ASSERT_TRUE(process_reader.Memory()->Read(
      FromPointerCast<mach_vm_address_t>(kTestMemory),
      sizeof(kTestMemory),
      &buffer));
  EXPECT_STREQ(kTestMemory, buffer);
}

constexpr char kTestMemory[] = "Read me from another process";

class ProcessReaderChild final : public MachMultiprocess {
 public:
  ProcessReaderChild() : MachMultiprocess() {}

  ProcessReaderChild(const ProcessReaderChild&) = delete;
  ProcessReaderChild& operator=(const ProcessReaderChild&) = delete;

  ~ProcessReaderChild() {}

 private:
  void MachMultiprocessParent() override {
    ProcessReaderMac process_reader;
    ASSERT_TRUE(process_reader.Initialize(ChildTask()));

#if !defined(ARCH_CPU_64_BITS)
    EXPECT_FALSE(process_reader.Is64Bit());
#else
    EXPECT_TRUE(process_reader.Is64Bit());
#endif

    EXPECT_EQ(process_reader.ParentProcessID(), getpid());
    EXPECT_EQ(process_reader.ProcessID(), ChildPID());

    FileHandle read_handle = ReadPipeHandle();

    mach_vm_address_t address;
    CheckedReadFileExactly(read_handle, &address, sizeof(address));

    std::string read_string;
    ASSERT_TRUE(process_reader.Memory()->ReadCString(address, &read_string));
    EXPECT_EQ(read_string, kTestMemory);
  }

  void MachMultiprocessChild() override {
    FileHandle write_handle = WritePipeHandle();

    mach_vm_address_t address = FromPointerCast<mach_vm_address_t>(kTestMemory);
    CheckedWriteFile(write_handle, &address, sizeof(address));

    // Wait for the parent to signal that it’s OK to exit by closing its end of
    // the pipe.
    CheckedReadFileAtEOF(ReadPipeHandle());
  }
};

TEST(ProcessReaderMac, ChildBasic) {
  ProcessReaderChild process_reader_child;
  process_reader_child.Run();
}

// Returns a thread ID given a pthread_t. This wraps pthread_threadid_np() but
// that function has a cumbersome interface because it returns a success value.
// This function CHECKs success and returns the thread ID directly.
uint64_t PthreadToThreadID(pthread_t pthread) {
  uint64_t thread_id;
  errno = pthread_threadid_np(pthread, &thread_id);
  PCHECK(errno == 0) << "pthread_threadid_np";
  return thread_id;
}

TEST(ProcessReaderMac, SelfOneThread) {
  const ScopedSetThreadName scoped_set_thread_name(
      "ProcessReaderMac/SelfOneThread");

  ProcessReaderMac process_reader;
  ASSERT_TRUE(process_reader.Initialize(mach_task_self()));

  const std::vector<ProcessReaderMac::Thread>& threads =
      process_reader.Threads();

  // If other tests ran in this process previously, threads may have been
  // created and may still be running. This check must look for at least one
  // thread, not exactly one thread.
  ASSERT_GE(threads.size(), 1u);

  EXPECT_EQ(threads[0].id, PthreadToThreadID(pthread_self()));
  EXPECT_EQ(threads[0].name, "ProcessReaderMac/SelfOneThread");

  thread_t thread_self = MachThreadSelf();
  EXPECT_EQ(threads[0].port, thread_self);

  EXPECT_EQ(threads[0].suspend_count, 0);
}

class TestThreadPool {
 public:
  struct ThreadExpectation {
    // The stack's base (highest) address.
    mach_vm_address_t stack_base;

    // The stack's maximum size.
    mach_vm_size_t stack_size;

    int suspend_count;
    std::string thread_name;
  };

  TestThreadPool(const std::string& thread_name_prefix)
      : thread_infos_(), thread_name_prefix_(thread_name_prefix) {}

  TestThreadPool(const TestThreadPool&) = delete;
  TestThreadPool& operator=(const TestThreadPool&) = delete;

  // Resumes suspended threads, signals each thread’s exit semaphore asking it
  // to exit, and joins each thread, blocking until they have all exited.
  ~TestThreadPool() {
    for (const auto& thread_info : thread_infos_) {
      thread_t thread_port = pthread_mach_thread_np(thread_info->pthread);
      while (thread_info->suspend_count > 0) {
        kern_return_t kr = thread_resume(thread_port);
        EXPECT_EQ(kr, KERN_SUCCESS) << MachErrorMessage(kr, "thread_resume");
        --thread_info->suspend_count;
      }
    }

    for (const auto& thread_info : thread_infos_) {
      thread_info->exit_semaphore.Signal();
    }

    for (const auto& thread_info : thread_infos_) {
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
      std::string thread_name = base::StringPrintf(
          "%s-%zu", thread_name_prefix_.c_str(), thread_index);
      thread_infos_.push_back(
          std::make_unique<ThreadInfo>(std::move(thread_name)));
      ThreadInfo* thread_info = thread_infos_.back().get();

      int rv = pthread_create(
          &thread_info->pthread, nullptr, ThreadMain, thread_info);
      ASSERT_EQ(rv, 0);
    }

    for (const auto& thread_info : thread_infos_) {
      thread_info->ready_semaphore.Wait();
    }

    // If present, suspend the thread at indices 1 through 3 the same number of
    // times as their index. This tests reporting of suspend counts.
    for (size_t thread_index = 1;
         thread_index < thread_infos_.size() && thread_index < 4;
         ++thread_index) {
      thread_t thread_port =
          pthread_mach_thread_np(thread_infos_[thread_index]->pthread);
      for (size_t suspend_count = 0; suspend_count < thread_index;
           ++suspend_count) {
        kern_return_t kr = thread_suspend(thread_port);
        EXPECT_EQ(kr, KERN_SUCCESS) << MachErrorMessage(kr, "thread_suspend");
        if (kr == KERN_SUCCESS) {
          ++thread_infos_[thread_index]->suspend_count;
        }
      }
    }
  }

  uint64_t GetThreadInfo(size_t thread_index, ThreadExpectation* expectation) {
    CHECK_LT(thread_index, thread_infos_.size());

    const auto& thread_info = thread_infos_[thread_index];
    expectation->stack_base = thread_info->stack_base;
    expectation->stack_size = thread_info->stack_size;
    expectation->suspend_count = thread_info->suspend_count;
    expectation->thread_name = thread_info->thread_name;

    return PthreadToThreadID(thread_info->pthread);
  }

 private:
  struct ThreadInfo {
    ThreadInfo(const std::string& thread_name)
        : pthread(nullptr),
          stack_base(0),
          stack_size(0),
          ready_semaphore(0),
          exit_semaphore(0),
          suspend_count(0),
          thread_name(thread_name) {}

    ~ThreadInfo() {}

    // The thread’s ID, set at the time the thread is created.
    pthread_t pthread;

    // The base address of thread’s stack. The thread sets this in
    // its ThreadMain().
    mach_vm_address_t stack_base;

    // The stack's maximum size. The thread sets this in its ThreadMain().
    mach_vm_size_t stack_size;

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

    // The thread's name.
    const std::string thread_name;
  };

  static void* ThreadMain(void* argument) {
    ThreadInfo* thread_info = static_cast<ThreadInfo*>(argument);
    const ScopedSetThreadName scoped_set_thread_name(thread_info->thread_name);

    pthread_t thread = pthread_self();
    thread_info->stack_base =
        FromPointerCast<mach_vm_address_t>(pthread_get_stackaddr_np(thread));
    thread_info->stack_size = pthread_get_stacksize_np(thread);

    thread_info->ready_semaphore.Signal();
    thread_info->exit_semaphore.Wait();

    // Check this here after everything’s known to be synchronized, otherwise
    // there’s a race between the parent thread storing this thread’s pthread_t
    // in thread_info_pthread and this thread starting and attempting to access
    // it.
    CHECK_EQ(pthread_self(), thread_info->pthread);

    return nullptr;
  }

  // This is a vector of pointers because the address of a ThreadInfo object is
  // passed to each thread’s ThreadMain(), so they cannot move around in memory.
  std::vector<std::unique_ptr<ThreadInfo>> thread_infos_;

  // Prefix to use for each thread's name, suffixed with "-$threadindex".
  const std::string thread_name_prefix_;
};

using ThreadMap = std::map<uint64_t, TestThreadPool::ThreadExpectation>;

// Verifies that all of the threads in |threads|, obtained from
// ProcessReaderMac, agree with the expectation in |thread_map|. If
// |tolerate_extra_threads| is true, |threads| is allowed to contain threads
// that are not listed in |thread_map|. This is useful when testing situations
// where code outside of the test’s control (such as system libraries) may start
// threads, or may have started threads prior to a test’s execution.
void ExpectSeveralThreads(ThreadMap* thread_map,
                          const std::vector<ProcessReaderMac::Thread>& threads,
                          const bool tolerate_extra_threads) {
  if (tolerate_extra_threads) {
    ASSERT_GE(threads.size(), thread_map->size());
  } else {
    ASSERT_EQ(threads.size(), thread_map->size());
  }

  for (size_t thread_index = 0; thread_index < threads.size(); ++thread_index) {
    const ProcessReaderMac::Thread& thread = threads[thread_index];
    mach_vm_address_t thread_stack_region_end =
        thread.stack_region_address + thread.stack_region_size;

    const auto& iterator = thread_map->find(thread.id);
    if (!tolerate_extra_threads) {
      // Make sure that the thread is in the expectation map.
      ASSERT_NE(iterator, thread_map->end());
    }

    if (iterator != thread_map->end()) {
      mach_vm_address_t expected_stack_region_end = iterator->second.stack_base;
      if (thread_index > 0) {
        // Non-main threads use the stack region to store thread data. See
        // macOS 12 libpthread-486.100.11 src/pthread.c _pthread_allocate().
#if defined(ARCH_CPU_ARM64)
        // arm64 has an additional offset for alignment. See macOS 12
        // libpthread-486.100.11 src/pthread.c _pthread_allocate() and
        // PTHREAD_T_OFFSET (defined in src/types_internal.h).
        expected_stack_region_end += sizeof(_opaque_pthread_t) + 0x3000;
#else
        expected_stack_region_end += sizeof(_opaque_pthread_t);
#endif
      }
      EXPECT_LT(iterator->second.stack_base - iterator->second.stack_size,
                thread.stack_region_address);
      EXPECT_EQ(expected_stack_region_end, thread_stack_region_end);

      EXPECT_EQ(thread.suspend_count, iterator->second.suspend_count);
      EXPECT_EQ(thread.name, iterator->second.thread_name);

      // Remove the thread from the expectation map since it’s already been
      // found. This makes it easy to check for duplicate thread IDs, and makes
      // it easy to check that all expected threads were found.
      thread_map->erase(iterator);
    }

    // Make sure that this thread’s ID, stack region, and port don’t conflict
    // with any other thread’s. Each thread should have a unique value for its
    // ID and port, and each should have its own stack that doesn’t touch any
    // other thread’s stack.
    for (size_t other_thread_index = 0; other_thread_index < threads.size();
         ++other_thread_index) {
      if (other_thread_index == thread_index) {
        continue;
      }

      const ProcessReaderMac::Thread& other_thread =
          threads[other_thread_index];

      EXPECT_NE(other_thread.id, thread.id);
      EXPECT_NE(other_thread.port, thread.port);

      mach_vm_address_t other_thread_stack_region_end =
          other_thread.stack_region_address + other_thread.stack_region_size;
      EXPECT_FALSE(thread.stack_region_address >=
                       other_thread.stack_region_address &&
                   thread.stack_region_address < other_thread_stack_region_end);
      EXPECT_FALSE(thread_stack_region_end >
                       other_thread.stack_region_address &&
                   thread_stack_region_end <= other_thread_stack_region_end);
    }
  }

  // Make sure that each expected thread was found.
  EXPECT_TRUE(thread_map->empty());
}

TEST(ProcessReaderMac, SelfSeveralThreads) {
  // Set up the ProcessReaderMac here, before any other threads are running.
  // This tests that the threads it returns are lazily initialized as a snapshot
  // of the threads at the time of the first call to Threads(), and not at the
  // time the ProcessReader was created or initialized.
  ProcessReaderMac process_reader;
  ASSERT_TRUE(process_reader.Initialize(mach_task_self()));

  TestThreadPool thread_pool("SelfSeveralThreads");
  constexpr size_t kChildThreads = 16;
  ASSERT_NO_FATAL_FAILURE(thread_pool.StartThreads(kChildThreads));

  // Build a map of all expected threads, keyed by each thread’s ID. The values
  // are addresses that should lie somewhere within each thread’s stack.
  ThreadMap thread_map;
  const uint64_t self_thread_id = PthreadToThreadID(pthread_self());
  TestThreadPool::ThreadExpectation expectation;
  expectation.stack_base = FromPointerCast<mach_vm_address_t>(
      pthread_get_stackaddr_np(pthread_self()));
  expectation.stack_size = pthread_get_stacksize_np(pthread_self());
  expectation.suspend_count = 0;
  thread_map[self_thread_id] = expectation;
  for (size_t thread_index = 0; thread_index < kChildThreads; ++thread_index) {
    uint64_t thread_id = thread_pool.GetThreadInfo(thread_index, &expectation);

    // There can’t be any duplicate thread IDs.
    EXPECT_EQ(thread_map.count(thread_id), 0u);

    expectation.thread_name =
        base::StringPrintf("SelfSeveralThreads-%zu", thread_index);
    thread_map[thread_id] = expectation;
  }

  const std::vector<ProcessReaderMac::Thread>& threads =
      process_reader.Threads();

  // Other tests that have run previously may have resulted in the creation of
  // threads that still exist, so pass true for |tolerate_extra_threads|.
  ExpectSeveralThreads(&thread_map, threads, true);

  // When testing in-process, verify that when this thread shows up in the
  // vector, it has the expected thread port, and that this thread port only
  // shows up once.
  thread_t thread_self = MachThreadSelf();
  bool found_thread_self = false;
  for (const ProcessReaderMac::Thread& thread : threads) {
    if (thread.port == thread_self) {
      EXPECT_FALSE(found_thread_self);
      found_thread_self = true;
      EXPECT_EQ(thread.id, self_thread_id);
    }
  }
  EXPECT_TRUE(found_thread_self);
}

uint64_t GetThreadID() {
  thread_identifier_info info;
  mach_msg_type_number_t info_count = THREAD_IDENTIFIER_INFO_COUNT;
  kern_return_t kr = thread_info(MachThreadSelf(),
                                 THREAD_IDENTIFIER_INFO,
                                 reinterpret_cast<thread_info_t>(&info),
                                 &info_count);
  MACH_CHECK(kr == KERN_SUCCESS, kr) << "thread_info";

  return info.thread_id;
}

class ProcessReaderThreadedChild final : public MachMultiprocess {
 public:
  explicit ProcessReaderThreadedChild(const std::string thread_name_prefix,
                                      size_t thread_count)
      : MachMultiprocess(),
        thread_name_prefix_(thread_name_prefix),
        thread_count_(thread_count) {}

  ProcessReaderThreadedChild(const ProcessReaderThreadedChild&) = delete;
  ProcessReaderThreadedChild& operator=(const ProcessReaderThreadedChild&) =
      delete;

  ~ProcessReaderThreadedChild() {}

 private:
  void MachMultiprocessParent() override {
    ProcessReaderMac process_reader;
    ASSERT_TRUE(process_reader.Initialize(ChildTask()));

    FileHandle read_handle = ReadPipeHandle();

    // Build a map of all expected threads, keyed by each thread’s ID, and with
    // addresses that should lie somewhere within each thread’s stack as values.
    // These IDs and addresses all come from the child process via the pipe.
    ThreadMap thread_map;
    for (size_t thread_index = 0; thread_index < thread_count_ + 1;
         ++thread_index) {
      uint64_t thread_id;
      CheckedReadFileExactly(read_handle, &thread_id, sizeof(thread_id));

      TestThreadPool::ThreadExpectation expectation;
      CheckedReadFileExactly(
          read_handle, &expectation.stack_base, sizeof(expectation.stack_base));
      CheckedReadFileExactly(
          read_handle, &expectation.stack_size, sizeof(expectation.stack_size));
      CheckedReadFileExactly(read_handle,
                             &expectation.suspend_count,
                             sizeof(expectation.suspend_count));
      std::string::size_type expected_thread_name_length;
      CheckedReadFileExactly(read_handle,
                             &expected_thread_name_length,
                             sizeof(expected_thread_name_length));
      std::string expected_thread_name(expected_thread_name_length, '\0');
      CheckedReadFileExactly(read_handle,
                             expected_thread_name.data(),
                             expected_thread_name_length);
      expectation.thread_name = expected_thread_name;

      // There can’t be any duplicate thread IDs.
      EXPECT_EQ(thread_map.count(thread_id), 0u);

      thread_map[thread_id] = expectation;
    }

    const std::vector<ProcessReaderMac::Thread>& threads =
        process_reader.Threads();

    // The child shouldn’t have any threads other than its main thread and the
    // ones it created in its pool, so pass false for |tolerate_extra_threads|.
    ExpectSeveralThreads(&thread_map, threads, false);
  }

  void MachMultiprocessChild() override {
    TestThreadPool thread_pool(thread_name_prefix_);
    ASSERT_NO_FATAL_FAILURE(thread_pool.StartThreads(thread_count_));

    const std::string current_thread_name(base::StringPrintf(
        "%s-MachMultiprocessChild", thread_name_prefix_.c_str()));
    const ScopedSetThreadName scoped_set_thread_name(current_thread_name);

    FileHandle write_handle = WritePipeHandle();

    // This thread isn’t part of the thread pool, but the parent will be able
    // to inspect it. Write an entry for it.
    uint64_t thread_id = GetThreadID();

    CheckedWriteFile(write_handle, &thread_id, sizeof(thread_id));

    TestThreadPool::ThreadExpectation expectation;
    pthread_t thread = pthread_self();
    expectation.stack_base =
        FromPointerCast<mach_vm_address_t>(pthread_get_stackaddr_np(thread));
    expectation.stack_size = pthread_get_stacksize_np(thread);
    expectation.suspend_count = 0;

    CheckedWriteFile(
        write_handle, &expectation.stack_base, sizeof(expectation.stack_base));
    CheckedWriteFile(
        write_handle, &expectation.stack_size, sizeof(expectation.stack_size));
    CheckedWriteFile(write_handle,
                     &expectation.suspend_count,
                     sizeof(expectation.suspend_count));
    const std::string::size_type current_thread_name_length =
        current_thread_name.length();
    CheckedWriteFile(write_handle,
                     &current_thread_name_length,
                     sizeof(current_thread_name_length));
    CheckedWriteFile(
        write_handle, current_thread_name.data(), current_thread_name_length);

    // Write an entry for everything in the thread pool.
    for (size_t thread_index = 0; thread_index < thread_count_;
         ++thread_index) {
      thread_id = thread_pool.GetThreadInfo(thread_index, &expectation);

      CheckedWriteFile(write_handle, &thread_id, sizeof(thread_id));
      CheckedWriteFile(write_handle,
                       &expectation.stack_base,
                       sizeof(expectation.stack_base));
      CheckedWriteFile(write_handle,
                       &expectation.stack_size,
                       sizeof(expectation.stack_size));
      CheckedWriteFile(write_handle,
                       &expectation.suspend_count,
                       sizeof(expectation.suspend_count));
      const std::string thread_pool_thread_name = base::StringPrintf(
          "%s-%zu", thread_name_prefix_.c_str(), thread_index);
      const std::string::size_type thread_pool_thread_name_length =
          thread_pool_thread_name.length();
      CheckedWriteFile(write_handle,
                       &thread_pool_thread_name_length,
                       sizeof(thread_pool_thread_name_length));
      CheckedWriteFile(write_handle,
                       thread_pool_thread_name.data(),
                       thread_pool_thread_name_length);
    }

    // Wait for the parent to signal that it’s OK to exit by closing its end of
    // the pipe.
    CheckedReadFileAtEOF(ReadPipeHandle());
  }

  const std::string thread_name_prefix_;
  size_t thread_count_;
};

TEST(ProcessReaderMac, ChildOneThread) {
  // The main thread plus zero child threads equals one thread.
  constexpr size_t kChildThreads = 0;
  ProcessReaderThreadedChild process_reader_threaded_child("ChildOneThread",
                                                           kChildThreads);
  process_reader_threaded_child.Run();
}

TEST(ProcessReaderMac, ChildSeveralThreads) {
  constexpr size_t kChildThreads = 64;
  ProcessReaderThreadedChild process_reader_threaded_child(
      "ChildSeveralThreads", kChildThreads);
  process_reader_threaded_child.Run();
}

template <typename T>
T GetDyldFunction(const char* symbol) {
  static void* dl_handle = []() -> void* {
    Dl_info dl_info;
    if (!dladdr(reinterpret_cast<void*>(dlopen), &dl_info)) {
      LOG(ERROR) << "dladdr: failed";
      return nullptr;
    }

    void* dl_handle =
        dlopen(dl_info.dli_fname, RTLD_LAZY | RTLD_LOCAL | RTLD_NOLOAD);
    DCHECK(dl_handle) << "dlopen: " << dlerror();

    return dl_handle;
  }();

  if (!dl_handle) {
    return nullptr;
  }

  return reinterpret_cast<T>(dlsym(dl_handle, symbol));
}

void VerifyImageExistence(const char* path) {
  const char* stat_path;

#if __MAC_OS_X_VERSION_MAX_ALLOWED < __MAC_10_16
  static auto _dyld_shared_cache_contains_path =
      GetDyldFunction<bool (*)(const char*)>(
          "_dyld_shared_cache_contains_path");
#endif

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"
  if (&_dyld_shared_cache_contains_path &&
      _dyld_shared_cache_contains_path(path)) {
#pragma clang diagnostic pop
    // The timestamp will either match the timestamp of the dyld_shared_cache
    // file in use, or be 0.
    static const char* dyld_shared_cache_file_path = []() -> const char* {
      auto dyld_shared_cache_file_path_f =
          GetDyldFunction<const char* (*)()>("dyld_shared_cache_file_path");

      // dyld_shared_cache_file_path should always be present if
      // _dyld_shared_cache_contains_path is.
      DCHECK(dyld_shared_cache_file_path_f);

      const char* dyld_shared_cache_file_path = dyld_shared_cache_file_path_f();
      DCHECK(dyld_shared_cache_file_path);

      return dyld_shared_cache_file_path;
    }();

    stat_path = dyld_shared_cache_file_path;
  } else {
    stat_path = path;
  }

  struct stat stat_buf;
  int rv = stat(stat_path, &stat_buf);
  EXPECT_EQ(rv, 0) << ErrnoMessage("stat");
}

// cl_kernels images (OpenCL kernels) are weird. They’re not ld output and don’t
// exist as files on disk. On OS X 10.10 and 10.11, their Mach-O structure isn’t
// perfect. They show up loaded into many executables, so these quirks should be
// tolerated.
//
// Create an object of this class to ensure that at least one cl_kernels image
// is present in a process, to be able to test that all of the process-reading
// machinery tolerates them. On systems where cl_kernels modules have known
// quirks, the image that an object of this class produces will also have those
// quirks.
//
// https://openradar.appspot.com/20239912
class ScopedOpenCLNoOpKernel {
 public:
  ScopedOpenCLNoOpKernel()
      : context_(nullptr),
        program_(nullptr),
        kernel_(nullptr),
        success_(false) {}

  ScopedOpenCLNoOpKernel(const ScopedOpenCLNoOpKernel&) = delete;
  ScopedOpenCLNoOpKernel& operator=(const ScopedOpenCLNoOpKernel&) = delete;

  ~ScopedOpenCLNoOpKernel() {
    if (kernel_) {
      cl_int rv = clReleaseKernel(kernel_);
      EXPECT_EQ(rv, CL_SUCCESS) << "clReleaseKernel";
    }

    if (program_) {
      cl_int rv = clReleaseProgram(program_);
      EXPECT_EQ(rv, CL_SUCCESS) << "clReleaseProgram";
    }

    if (context_) {
      cl_int rv = clReleaseContext(context_);
      EXPECT_EQ(rv, CL_SUCCESS) << "clReleaseContext";
    }
  }

  void SetUp() {
    cl_platform_id platform_id;
    cl_int rv = clGetPlatformIDs(1, &platform_id, nullptr);
    ASSERT_EQ(rv, CL_SUCCESS) << "clGetPlatformIDs";

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= __MAC_10_10 && \
    __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_10
    // cl_device_id is really available in OpenCL.framework back to 10.5, but in
    // the 10.10 SDK and later, OpenCL.framework includes <OpenGL/CGLDevice.h>,
    // which has its own cl_device_id that was introduced in 10.10. That
    // triggers erroneous availability warnings.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability"
#define DISABLED_WUNGUARDED_AVAILABILITY
#endif  // SDK >= 10.10 && DT < 10.10
    // Use CL_DEVICE_TYPE_CPU to ensure that the kernel would execute on the
    // CPU. This is the only device type that a cl_kernels image will be created
    // for.
    cl_device_id device_id;
#if defined(DISABLED_WUNGUARDED_AVAILABILITY)
#pragma clang diagnostic pop
#undef DISABLED_WUNGUARDED_AVAILABILITY
#endif  // DISABLED_WUNGUARDED_AVAILABILITY
    rv =
        clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_CPU, 1, &device_id, nullptr);
#if defined(ARCH_CPU_ARM64)
    // CL_DEVICE_TYPE_CPU doesn’t seem to work at all on arm64, meaning that
    // these weird OpenCL modules probably don’t show up there at all. Keep this
    // test even on arm64 in case this ever does start working.
    if (rv == CL_INVALID_VALUE) {
      return;
    }
#endif  // ARCH_CPU_ARM64
    ASSERT_EQ(rv, CL_SUCCESS) << "clGetDeviceIDs";

    context_ = clCreateContext(nullptr, 1, &device_id, nullptr, nullptr, &rv);
    ASSERT_EQ(rv, CL_SUCCESS) << "clCreateContext";

    // The goal of the program in |sources| is to produce a cl_kernels image
    // that doesn’t strictly conform to Mach-O expectations. On OS X 10.10,
    // cl_kernels modules show up with an __LD,__compact_unwind section, showing
    // up in the __TEXT segment. MachOImageSegmentReader would normally reject
    // modules for this problem, but a special exception is made when this
    // occurs in cl_kernels images. This portion of the test is aimed at making
    // sure that this exception works correctly.
    //
    // A true no-op program doesn’t actually produce unwind data, so there would
    // be no errant __LD,__compact_unwind section on 10.10, and the test
    // wouldn’t be complete. This simple no-op, which calls a built-in function,
    // does produce unwind data provided optimization is disabled.
    // "-cl-opt-disable" is given to clBuildProgram() below.
    const char* sources[] = {
        "__kernel void NoOp(void) {barrier(CLK_LOCAL_MEM_FENCE);}",
    };
    const size_t source_lengths[] = {
        strlen(sources[0]),
    };
    static_assert(std::size(sources) == std::size(source_lengths),
                  "arrays must be parallel");

    program_ = clCreateProgramWithSource(
        context_, std::size(sources), sources, source_lengths, &rv);
    ASSERT_EQ(rv, CL_SUCCESS) << "clCreateProgramWithSource";

    rv = clBuildProgram(
        program_, 1, &device_id, "-cl-opt-disable", nullptr, nullptr);
    ASSERT_EQ(rv, CL_SUCCESS) << "clBuildProgram";

    kernel_ = clCreateKernel(program_, "NoOp", &rv);
    ASSERT_EQ(rv, CL_SUCCESS) << "clCreateKernel";

    success_ = true;
  }

  bool success() const { return success_; }

 private:
  cl_context context_;
  cl_program program_;
  cl_kernel kernel_;
  bool success_;
};

// Although Mac OS X 10.6 has OpenCL and can compile and execute OpenCL code,
// OpenCL kernels that run on the CPU do not result in cl_kernels images
// appearing on that OS version.
bool ExpectCLKernels() {
  return __MAC_OS_X_VERSION_MIN_REQUIRED >= __MAC_10_7 ||
         MacOSVersionNumber() >= 10'07'00;
}

// Starting in dyld-1284.13 (macOS 15.4), _dyld_get_image_name (in
// dyld4::PrebuiltLoader::path) returns an absolute path for a main executable,
// even when the image was not loaded from an absolute path. This can break test
// expectations when the test executable is not invoked from an absolute path.
// As a workaround, just use the main executable’s basename for comparison
// purposes.
std::string ComparableModuleName(const std::string& module_name,
                                 uint32_t file_type) {
  return file_type == MH_EXECUTE
             ? base::FilePath(module_name).BaseName().value()
             : module_name;
}

TEST(ProcessReaderMac, SelfModules) {
  ScopedOpenCLNoOpKernel ensure_cl_kernels;
  ASSERT_NO_FATAL_FAILURE(ensure_cl_kernels.SetUp());

  ProcessReaderMac process_reader;
  ASSERT_TRUE(process_reader.Initialize(mach_task_self()));

  uint32_t dyld_image_count = _dyld_image_count();

  std::set<std::string> cl_kernel_names;
  auto modules = process_reader.Modules();
  ModuleSet actual_modules;
  for (size_t i = 0; i < modules.size(); ++i) {
    auto& module = modules[i];
    ASSERT_TRUE(module.reader);
    if (i == modules.size() - 1) {
      EXPECT_EQ(module.name, kDyldPath);
      const dyld_all_image_infos* dyld_image_infos = DyldGetAllImageInfos();
      if (dyld_image_infos->version >= 2) {
        EXPECT_EQ(module.reader->Address(),
                  FromPointerCast<mach_vm_address_t>(
                      dyld_image_infos->dyldImageLoadAddress));
      }
      // Don't include dyld, since dyld image APIs will not have an entry for
      // dyld itself.
      continue;
    }
    // Ensure executable is first, and that there's only one.
    uint32_t file_type = module.reader->FileType();
    if (i == 0) {
      EXPECT_EQ(file_type, static_cast<uint32_t>(MH_EXECUTE));
    } else {
      EXPECT_NE(file_type, static_cast<uint32_t>(MH_EXECUTE));
      EXPECT_NE(file_type, static_cast<uint32_t>(MH_DYLINKER));
    }
    if (IsMalformedCLKernelsModule(module.reader->FileType(), module.name)) {
      cl_kernel_names.insert(module.name);
    }
    actual_modules.insert(
        std::make_pair(ComparableModuleName(module.name, file_type),
                       module.reader->Address()));
  }
  EXPECT_EQ(cl_kernel_names.size() > 0,
            ExpectCLKernels() && ensure_cl_kernels.success());

  // There needs to be at least an entry for the main executable and a dylib.
  ASSERT_GE(actual_modules.size(), 2u);
  ASSERT_EQ(actual_modules.size(), dyld_image_count);

  ModuleSet expect_modules;
  for (uint32_t index = 0; index < dyld_image_count; ++index) {
    const char* dyld_image_name = _dyld_get_image_name(index);
    mach_vm_address_t dyld_image_address =
        FromPointerCast<mach_vm_address_t>(_dyld_get_image_header(index));
    const MachHeader* image_header =
        reinterpret_cast<const MachHeader*>(dyld_image_address);
    expect_modules.insert(std::make_pair(
        ComparableModuleName(dyld_image_name, image_header->filetype),
        dyld_image_address));
    if (cl_kernel_names.find(dyld_image_name) == cl_kernel_names.end()) {
      VerifyImageExistence(dyld_image_name);
    }
  }
  EXPECT_EQ(actual_modules, expect_modules);
}

class ProcessReaderModulesChild final : public MachMultiprocess {
 public:
  explicit ProcessReaderModulesChild(bool ensure_cl_kernels_success)
      : MachMultiprocess(),
        ensure_cl_kernels_success_(ensure_cl_kernels_success) {}

  ProcessReaderModulesChild(const ProcessReaderModulesChild&) = delete;
  ProcessReaderModulesChild& operator=(const ProcessReaderModulesChild&) =
      delete;

  ~ProcessReaderModulesChild() {}

 private:
  void MachMultiprocessParent() override {
    ProcessReaderMac process_reader;
    ASSERT_TRUE(process_reader.Initialize(ChildTask()));
    const std::vector<ProcessReaderMac::Module>& modules =
        process_reader.Modules();

    ModuleSet actual_modules;
    std::set<std::string> cl_kernel_names;
    for (size_t i = 0; i < modules.size(); ++i) {
      auto& module = modules[i];
      ASSERT_TRUE(module.reader);
      uint32_t file_type = module.reader->FileType();
      if (i == 0) {
        EXPECT_EQ(file_type, static_cast<uint32_t>(MH_EXECUTE));
      } else if (i == modules.size() - 1) {
        EXPECT_EQ(file_type, static_cast<uint32_t>(MH_DYLINKER));
      } else {
        EXPECT_NE(file_type, static_cast<uint32_t>(MH_EXECUTE));
        EXPECT_NE(file_type, static_cast<uint32_t>(MH_DYLINKER));
      }
      if (IsMalformedCLKernelsModule(module.reader->FileType(), module.name)) {
        cl_kernel_names.insert(module.name);
      }
      actual_modules.insert(
          std::make_pair(ComparableModuleName(module.name, file_type),
                         module.reader->Address()));
    }

    // There needs to be at least an entry for the main executable, for a dylib,
    // and for dyld.
    ASSERT_GE(actual_modules.size(), 3u);

    FileHandle read_handle = ReadPipeHandle();

    uint32_t expect_modules_size;
    CheckedReadFileExactly(
        read_handle, &expect_modules_size, sizeof(expect_modules_size));

    ASSERT_EQ(actual_modules.size(), expect_modules_size);
    ModuleSet expect_modules;

    for (size_t index = 0; index < expect_modules_size; ++index) {
      uint32_t expect_name_length;
      CheckedReadFileExactly(
          read_handle, &expect_name_length, sizeof(expect_name_length));

      // The NUL terminator is not read.
      std::string expect_name(expect_name_length, '\0');
      CheckedReadFileExactly(read_handle, &expect_name[0], expect_name_length);

      mach_vm_address_t expect_address;
      CheckedReadFileExactly(
          read_handle, &expect_address, sizeof(expect_address));

      uint32_t file_type;
      CheckedReadFileExactly(read_handle, &file_type, sizeof(file_type));

      if (cl_kernel_names.find(expect_name) == cl_kernel_names.end()) {
        VerifyImageExistence(expect_name.c_str());
      }

      expect_modules.insert(std::make_pair(
          ComparableModuleName(expect_name, file_type), expect_address));
    }
    EXPECT_EQ(cl_kernel_names.size() > 0,
              ExpectCLKernels() && ensure_cl_kernels_success_);
    EXPECT_EQ(expect_modules, actual_modules);
  }

  void MachMultiprocessChild() override {
    FileHandle write_handle = WritePipeHandle();

    uint32_t dyld_image_count = _dyld_image_count();
    const dyld_all_image_infos* dyld_image_infos = DyldGetAllImageInfos();

    uint32_t write_image_count = dyld_image_count;
    if (dyld_image_infos->version >= 2) {
      // dyld_image_count doesn’t include an entry for dyld itself, but one will
      // be written.
      ++write_image_count;
    }

    CheckedWriteFile(
        write_handle, &write_image_count, sizeof(write_image_count));

    for (size_t index = 0; index < write_image_count; ++index) {
      const char* dyld_image_name;
      mach_vm_address_t dyld_image_address;

      if (index < dyld_image_count) {
        dyld_image_name = _dyld_get_image_name(index);
        dyld_image_address =
            FromPointerCast<mach_vm_address_t>(_dyld_get_image_header(index));
      } else {
        dyld_image_name = kDyldPath;
        dyld_image_address = FromPointerCast<mach_vm_address_t>(
            dyld_image_infos->dyldImageLoadAddress);
      }

      const MachHeader* image_header =
          reinterpret_cast<const MachHeader*>(dyld_image_address);
      uint32_t file_type = image_header->filetype;

      uint32_t dyld_image_name_length = strlen(dyld_image_name);
      CheckedWriteFile(write_handle,
                       &dyld_image_name_length,
                       sizeof(dyld_image_name_length));

      // The NUL terminator is not written.
      CheckedWriteFile(write_handle, dyld_image_name, dyld_image_name_length);

      CheckedWriteFile(
          write_handle, &dyld_image_address, sizeof(dyld_image_address));
      CheckedWriteFile(write_handle, &file_type, sizeof(file_type));
    }

    // Wait for the parent to signal that it’s OK to exit by closing its end of
    // the pipe.
    CheckedReadFileAtEOF(ReadPipeHandle());
  }

  bool ensure_cl_kernels_success_;
};

TEST(ProcessReaderMac, ChildModules) {
  ScopedOpenCLNoOpKernel ensure_cl_kernels;
  ASSERT_NO_FATAL_FAILURE(ensure_cl_kernels.SetUp());

  ProcessReaderModulesChild process_reader_modules_child(
      ensure_cl_kernels.success());
  process_reader_modules_child.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
