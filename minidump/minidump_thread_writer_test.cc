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

#include "minidump/minidump_thread_writer.h"

#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/format_macros.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "gtest/gtest.h"
#include "minidump/minidump_context_writer.h"
#include "minidump/minidump_file_writer.h"
#include "minidump/minidump_memory_writer.h"
#include "minidump/test/minidump_context_test_util.h"
#include "minidump/test/minidump_file_writer_test_util.h"
#include "minidump/test/minidump_memory_writer_test_util.h"
#include "minidump/test/minidump_writable_test_util.h"
#include "snapshot/test/test_cpu_context.h"
#include "snapshot/test/test_memory_snapshot.h"
#include "snapshot/test/test_thread_snapshot.h"
#include "test/gtest_death.h"
#include "util/file/string_file.h"

namespace crashpad {
namespace test {
namespace {

// This returns the MINIDUMP_THREAD_LIST stream in |thread_list|. If
// |memory_list| is not nullptr, a MINIDUMP_MEMORY_LIST stream is also expected
// in |file_contents|, and that stream will be returned in |memory_list|.
void GetThreadListStream(const std::string& file_contents,
                         const MINIDUMP_THREAD_LIST** thread_list,
                         const MINIDUMP_MEMORY_LIST** memory_list) {
  constexpr size_t kDirectoryOffset = sizeof(MINIDUMP_HEADER);
  const uint32_t kExpectedStreams = memory_list ? 2 : 1;
  const size_t kThreadListStreamOffset =
      kDirectoryOffset + kExpectedStreams * sizeof(MINIDUMP_DIRECTORY);
  const size_t kThreadsOffset =
      kThreadListStreamOffset + sizeof(MINIDUMP_THREAD_LIST);

  ASSERT_GE(file_contents.size(), kThreadsOffset);

  const MINIDUMP_DIRECTORY* directory;
  const MINIDUMP_HEADER* header =
      MinidumpHeaderAtStart(file_contents, &directory);
  ASSERT_NO_FATAL_FAILURE(VerifyMinidumpHeader(header, kExpectedStreams, 0));
  ASSERT_TRUE(directory);

  ASSERT_EQ(directory[0].StreamType, kMinidumpStreamTypeThreadList);
  EXPECT_EQ(directory[0].Location.Rva, kThreadListStreamOffset);

  *thread_list = MinidumpWritableAtLocationDescriptor<MINIDUMP_THREAD_LIST>(
      file_contents, directory[0].Location);
  ASSERT_TRUE(thread_list);

  if (memory_list) {
    ASSERT_EQ(directory[1].StreamType, kMinidumpStreamTypeMemoryList);

    *memory_list = MinidumpWritableAtLocationDescriptor<MINIDUMP_MEMORY_LIST>(
        file_contents, directory[1].Location);
    ASSERT_TRUE(*memory_list);
  }
}

TEST(MinidumpThreadWriter, EmptyThreadList) {
  MinidumpFileWriter minidump_file_writer;
  auto thread_list_writer = std::make_unique<MinidumpThreadListWriter>();

  ASSERT_TRUE(minidump_file_writer.AddStream(std::move(thread_list_writer)));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  ASSERT_EQ(string_file.string().size(),
            sizeof(MINIDUMP_HEADER) + sizeof(MINIDUMP_DIRECTORY) +
                sizeof(MINIDUMP_THREAD_LIST));

  const MINIDUMP_THREAD_LIST* thread_list = nullptr;
  ASSERT_NO_FATAL_FAILURE(
      GetThreadListStream(string_file.string(), &thread_list, nullptr));

  EXPECT_EQ(thread_list->NumberOfThreads, 0u);
}

// The MINIDUMP_THREADs |expected| and |observed| are compared against each
// other using gtest assertions. If |stack| is not nullptr, |observed| is
// expected to contain a populated MINIDUMP_MEMORY_DESCRIPTOR in its Stack
// field, otherwise, its Stack field is expected to be zeroed out. The memory
// descriptor will be placed in |stack|. |observed| must contain a populated
// ThreadContext field. The context will be recovered from |file_contents| and
// stored in |context_base|.
void ExpectThread(const MINIDUMP_THREAD* expected,
                  const MINIDUMP_THREAD* observed,
                  const std::string& file_contents,
                  const MINIDUMP_MEMORY_DESCRIPTOR** stack,
                  const void** context_base) {
  EXPECT_EQ(observed->ThreadId, expected->ThreadId);
  EXPECT_EQ(observed->SuspendCount, expected->SuspendCount);
  EXPECT_EQ(observed->PriorityClass, expected->PriorityClass);
  EXPECT_EQ(observed->Priority, expected->Priority);
  EXPECT_EQ(observed->Teb, expected->Teb);

  EXPECT_EQ(observed->Stack.StartOfMemoryRange,
            expected->Stack.StartOfMemoryRange);
  EXPECT_EQ(observed->Stack.Memory.DataSize, expected->Stack.Memory.DataSize);
  if (stack) {
    ASSERT_NE(observed->Stack.Memory.DataSize, 0u);
    ASSERT_NE(observed->Stack.Memory.Rva, 0u);
    ASSERT_GE(file_contents.size(),
              observed->Stack.Memory.Rva + observed->Stack.Memory.DataSize);
    *stack = &observed->Stack;
  } else {
    EXPECT_EQ(observed->Stack.StartOfMemoryRange, 0u);
    EXPECT_EQ(observed->Stack.Memory.DataSize, 0u);
    EXPECT_EQ(observed->Stack.Memory.Rva, 0u);
  }

  EXPECT_EQ(observed->ThreadContext.DataSize, expected->ThreadContext.DataSize);
  ASSERT_NE(observed->ThreadContext.DataSize, 0u);
  ASSERT_NE(observed->ThreadContext.Rva, 0u);
  ASSERT_GE(file_contents.size(),
            observed->ThreadContext.Rva + expected->ThreadContext.DataSize);
  *context_base = &file_contents[observed->ThreadContext.Rva];
}

TEST(MinidumpThreadWriter, OneThread_x86_NoStack) {
  MinidumpFileWriter minidump_file_writer;
  auto thread_list_writer = std::make_unique<MinidumpThreadListWriter>();

  constexpr uint32_t kThreadID = 0x11111111;
  constexpr uint32_t kSuspendCount = 1;
  constexpr uint32_t kPriorityClass = 0x20;
  constexpr uint32_t kPriority = 10;
  constexpr uint64_t kTEB = 0x55555555;
  constexpr uint32_t kSeed = 123;

  auto thread_writer = std::make_unique<MinidumpThreadWriter>();
  thread_writer->SetThreadID(kThreadID);
  thread_writer->SetSuspendCount(kSuspendCount);
  thread_writer->SetPriorityClass(kPriorityClass);
  thread_writer->SetPriority(kPriority);
  thread_writer->SetTEB(kTEB);

  auto context_x86_writer = std::make_unique<MinidumpContextX86Writer>();
  InitializeMinidumpContextX86(context_x86_writer->context(), kSeed);
  thread_writer->SetContext(std::move(context_x86_writer));

  thread_list_writer->AddThread(std::move(thread_writer));
  ASSERT_TRUE(minidump_file_writer.AddStream(std::move(thread_list_writer)));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  ASSERT_EQ(string_file.string().size(),
            sizeof(MINIDUMP_HEADER) + sizeof(MINIDUMP_DIRECTORY) +
                sizeof(MINIDUMP_THREAD_LIST) + 1 * sizeof(MINIDUMP_THREAD) +
                1 * sizeof(MinidumpContextX86));

  const MINIDUMP_THREAD_LIST* thread_list = nullptr;
  ASSERT_NO_FATAL_FAILURE(
      GetThreadListStream(string_file.string(), &thread_list, nullptr));

  EXPECT_EQ(thread_list->NumberOfThreads, 1u);

  MINIDUMP_THREAD expected = {};
  expected.ThreadId = kThreadID;
  expected.SuspendCount = kSuspendCount;
  expected.PriorityClass = kPriorityClass;
  expected.Priority = kPriority;
  expected.Teb = kTEB;
  expected.ThreadContext.DataSize = sizeof(MinidumpContextX86);

  const MinidumpContextX86* observed_context = nullptr;
  ASSERT_NO_FATAL_FAILURE(
      ExpectThread(&expected,
                   &thread_list->Threads[0],
                   string_file.string(),
                   nullptr,
                   reinterpret_cast<const void**>(&observed_context)));

  ASSERT_NO_FATAL_FAILURE(
      ExpectMinidumpContextX86(kSeed, observed_context, false));
}

TEST(MinidumpThreadWriter, OneThread_AMD64_Stack) {
  MinidumpFileWriter minidump_file_writer;
  auto thread_list_writer = std::make_unique<MinidumpThreadListWriter>();

  constexpr uint32_t kThreadID = 0x22222222;
  constexpr uint32_t kSuspendCount = 2;
  constexpr uint32_t kPriorityClass = 0x30;
  constexpr uint32_t kPriority = 20;
  constexpr uint64_t kTEB = 0x5555555555555555;
  constexpr uint64_t kMemoryBase = 0x765432100000;
  constexpr size_t kMemorySize = 32;
  constexpr uint8_t kMemoryValue = 99;
  constexpr uint32_t kSeed = 456;

  auto thread_writer = std::make_unique<MinidumpThreadWriter>();
  thread_writer->SetThreadID(kThreadID);
  thread_writer->SetSuspendCount(kSuspendCount);
  thread_writer->SetPriorityClass(kPriorityClass);
  thread_writer->SetPriority(kPriority);
  thread_writer->SetTEB(kTEB);

  auto memory_writer = std::make_unique<TestMinidumpMemoryWriter>(
      kMemoryBase, kMemorySize, kMemoryValue);
  thread_writer->SetStack(std::move(memory_writer));

  auto context_amd64_writer = std::make_unique<MinidumpContextAMD64Writer>();
  InitializeMinidumpContextAMD64(context_amd64_writer->context(), kSeed);
  thread_writer->SetContext(std::move(context_amd64_writer));

  thread_list_writer->AddThread(std::move(thread_writer));
  ASSERT_TRUE(minidump_file_writer.AddStream(std::move(thread_list_writer)));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  ASSERT_EQ(string_file.string().size(),
            sizeof(MINIDUMP_HEADER) + sizeof(MINIDUMP_DIRECTORY) +
                sizeof(MINIDUMP_THREAD_LIST) + 1 * sizeof(MINIDUMP_THREAD) +
                1 * sizeof(MinidumpContextAMD64) + kMemorySize);

  const MINIDUMP_THREAD_LIST* thread_list = nullptr;
  ASSERT_NO_FATAL_FAILURE(
      GetThreadListStream(string_file.string(), &thread_list, nullptr));

  EXPECT_EQ(thread_list->NumberOfThreads, 1u);

  MINIDUMP_THREAD expected = {};
  expected.ThreadId = kThreadID;
  expected.SuspendCount = kSuspendCount;
  expected.PriorityClass = kPriorityClass;
  expected.Priority = kPriority;
  expected.Teb = kTEB;
  expected.Stack.StartOfMemoryRange = kMemoryBase;
  expected.Stack.Memory.DataSize = kMemorySize;
  expected.ThreadContext.DataSize = sizeof(MinidumpContextAMD64);

  const MINIDUMP_MEMORY_DESCRIPTOR* observed_stack = nullptr;
  const MinidumpContextAMD64* observed_context = nullptr;
  ASSERT_NO_FATAL_FAILURE(
      ExpectThread(&expected,
                   &thread_list->Threads[0],
                   string_file.string(),
                   &observed_stack,
                   reinterpret_cast<const void**>(&observed_context)));

  ASSERT_NO_FATAL_FAILURE(
      ExpectMinidumpMemoryDescriptorAndContents(&expected.Stack,
                                                observed_stack,
                                                string_file.string(),
                                                kMemoryValue,
                                                true));
  ASSERT_NO_FATAL_FAILURE(
      ExpectMinidumpContextAMD64(kSeed, observed_context, false));
}

class MinidumpThreadWriterX86Test : public testing::Test {
 public:
  static constexpr uint32_t kThreadID0 = 1111111;
  static constexpr uint32_t kSuspendCount0 = 111111;
  static constexpr uint32_t kPriorityClass0 = 11111;
  static constexpr uint32_t kPriority0 = 1111;
  static constexpr uint64_t kTEB0 = 111;
  static constexpr uint64_t kMemoryBase0 = 0x1110;
  static constexpr size_t kMemorySize0 = 16;
  static constexpr uint8_t kMemoryValue0 = 11;
  static constexpr uint32_t kSeed0 = 1;

  static constexpr uint32_t kThreadID1 = 2222222;
  static constexpr uint32_t kSuspendCount1 = 222222;
  static constexpr uint32_t kPriorityClass1 = 22222;
  static constexpr uint32_t kPriority1 = 2222;
  static constexpr uint64_t kTEB1 = 222;
  static constexpr uint64_t kMemoryBase1 = 0x2220;
  static constexpr size_t kMemorySize1 = 32;
  static constexpr uint8_t kMemoryValue1 = 22;
  static constexpr uint32_t kSeed1 = 2;

  static constexpr uint32_t kThreadID2 = 3333333;
  static constexpr uint32_t kSuspendCount2 = 333333;
  static constexpr uint32_t kPriorityClass2 = 33333;
  static constexpr uint32_t kPriority2 = 3333;
  static constexpr uint64_t kTEB2 = 333;
  static constexpr uint64_t kMemoryBase2 = 0x3330;
  static constexpr size_t kMemorySize2 = 48;
  static constexpr uint8_t kMemoryValue2 = 33;
  static constexpr uint32_t kSeed2 = 3;

  MinidumpThreadWriterX86Test()
      : thread_list_(nullptr), memory_list_(nullptr), string_file_() {}

  ~MinidumpThreadWriterX86Test() override = default;

  const StringFile& string_file() const { return string_file_; }
  const MINIDUMP_THREAD_LIST& thread_list() const { return *thread_list_; }
  const MINIDUMP_MEMORY_LIST& memory_list() const { return *memory_list_; }

  // testing::Test:
  void SetUp() override {
    thread_list_ = nullptr;
    memory_list_ = nullptr;
  }

  void InitThreadWriter(bool has_dump_thread) {
    MinidumpFileWriter minidump_file_writer;
    std::unique_ptr<MinidumpThreadListWriter> thread_list_writer =
        has_dump_thread ? std::make_unique<MinidumpThreadListWriter>(kThreadID1)
                        : std::make_unique<MinidumpThreadListWriter>();
    auto memory_list_writer = std::make_unique<MinidumpMemoryListWriter>();
    thread_list_writer->SetMemoryListWriter(memory_list_writer.get());

    auto thread_writer_0 = std::make_unique<MinidumpThreadWriter>();
    thread_writer_0->SetThreadID(kThreadID0);
    thread_writer_0->SetSuspendCount(kSuspendCount0);
    thread_writer_0->SetPriorityClass(kPriorityClass0);
    thread_writer_0->SetPriority(kPriority0);
    thread_writer_0->SetTEB(kTEB0);

    auto memory_writer_0 = std::make_unique<TestMinidumpMemoryWriter>(
        kMemoryBase0, kMemorySize0, kMemoryValue0);
    thread_writer_0->SetStack(std::move(memory_writer_0));

    auto context_x86_writer_0 = std::make_unique<MinidumpContextX86Writer>();
    InitializeMinidumpContextX86(context_x86_writer_0->context(), kSeed0);
    thread_writer_0->SetContext(std::move(context_x86_writer_0));

    thread_list_writer->AddThread(std::move(thread_writer_0));

    auto thread_writer_1 = std::make_unique<MinidumpThreadWriter>();
    thread_writer_1->SetThreadID(kThreadID1);
    thread_writer_1->SetSuspendCount(kSuspendCount1);
    thread_writer_1->SetPriorityClass(kPriorityClass1);
    thread_writer_1->SetPriority(kPriority1);
    thread_writer_1->SetTEB(kTEB1);

    auto memory_writer_1 = std::make_unique<TestMinidumpMemoryWriter>(
        kMemoryBase1, kMemorySize1, kMemoryValue1);
    thread_writer_1->SetStack(std::move(memory_writer_1));

    auto context_x86_writer_1 = std::make_unique<MinidumpContextX86Writer>();
    InitializeMinidumpContextX86(context_x86_writer_1->context(), kSeed1);
    thread_writer_1->SetContext(std::move(context_x86_writer_1));

    thread_list_writer->AddThread(std::move(thread_writer_1));

    auto thread_writer_2 = std::make_unique<MinidumpThreadWriter>();
    thread_writer_2->SetThreadID(kThreadID2);
    thread_writer_2->SetSuspendCount(kSuspendCount2);
    thread_writer_2->SetPriorityClass(kPriorityClass2);
    thread_writer_2->SetPriority(kPriority2);
    thread_writer_2->SetTEB(kTEB2);

    auto memory_writer_2 = std::make_unique<TestMinidumpMemoryWriter>(
        kMemoryBase2, kMemorySize2, kMemoryValue2);
    thread_writer_2->SetStack(std::move(memory_writer_2));
    auto context_x86_writer_2 = std::make_unique<MinidumpContextX86Writer>();
    InitializeMinidumpContextX86(context_x86_writer_2->context(), kSeed2);
    thread_writer_2->SetContext(std::move(context_x86_writer_2));

    thread_list_writer->AddThread(std::move(thread_writer_2));

    ASSERT_TRUE(minidump_file_writer.AddStream(std::move(thread_list_writer)));
    ASSERT_TRUE(minidump_file_writer.AddStream(std::move(memory_list_writer)));

    ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file_));
    auto thread_count = has_dump_thread ? 1 : 3;
    auto memory_size = has_dump_thread
                           ? kMemorySize1
                           : kMemorySize0 + kMemorySize1 + kMemorySize2;
    auto padding_size = has_dump_thread ? 4 : 12;  // for alignment
    ASSERT_EQ(string_file_.string().size(),
              sizeof(MINIDUMP_HEADER) + 2 * sizeof(MINIDUMP_DIRECTORY) +
                  sizeof(MINIDUMP_THREAD_LIST) +
                  thread_count * sizeof(MINIDUMP_THREAD) +
                  sizeof(MINIDUMP_MEMORY_LIST) +
                  thread_count * sizeof(MINIDUMP_MEMORY_DESCRIPTOR) +
                  thread_count * sizeof(MinidumpContextX86) + memory_size +
                  padding_size);

    ASSERT_NO_FATAL_FAILURE(GetThreadListStream(
        string_file_.string(), &thread_list_, &memory_list_));
    ASSERT_TRUE(thread_list_);
    ASSERT_TRUE(memory_list_);
  }

  const MINIDUMP_THREAD_LIST* thread_list_;
  const MINIDUMP_MEMORY_LIST* memory_list_;
  StringFile string_file_;

  DISALLOW_COPY_AND_ASSIGN(MinidumpThreadWriterX86Test);
};

TEST_F(MinidumpThreadWriterX86Test, ThreeThreads_x86_MemoryList) {
  InitThreadWriter(false /* has_dump_thread */);
  EXPECT_EQ(thread_list().NumberOfThreads, 3u);
  EXPECT_EQ(memory_list().NumberOfMemoryRanges, 3u);

  {
    SCOPED_TRACE("thread 0");

    MINIDUMP_THREAD expected = {};
    expected.ThreadId = kThreadID0;
    expected.SuspendCount = kSuspendCount0;
    expected.PriorityClass = kPriorityClass0;
    expected.Priority = kPriority0;
    expected.Teb = kTEB0;
    expected.Stack.StartOfMemoryRange = kMemoryBase0;
    expected.Stack.Memory.DataSize = kMemorySize0;
    expected.ThreadContext.DataSize = sizeof(MinidumpContextX86);

    const MINIDUMP_MEMORY_DESCRIPTOR* observed_stack = nullptr;
    const MinidumpContextX86* observed_context = nullptr;
    ASSERT_NO_FATAL_FAILURE(
        ExpectThread(&expected,
                     &thread_list().Threads[0],
                     string_file().string(),
                     &observed_stack,
                     reinterpret_cast<const void**>(&observed_context)));

    ASSERT_NO_FATAL_FAILURE(
        ExpectMinidumpMemoryDescriptorAndContents(&expected.Stack,
                                                  observed_stack,
                                                  string_file().string(),
                                                  kMemoryValue0,
                                                  false));
    ASSERT_NO_FATAL_FAILURE(
        ExpectMinidumpContextX86(kSeed0, observed_context, false));
    ASSERT_NO_FATAL_FAILURE(ExpectMinidumpMemoryDescriptor(
        observed_stack, &memory_list().MemoryRanges[0]));
  }

  {
    SCOPED_TRACE("thread 1");

    MINIDUMP_THREAD expected = {};
    expected.ThreadId = kThreadID1;
    expected.SuspendCount = kSuspendCount1;
    expected.PriorityClass = kPriorityClass1;
    expected.Priority = kPriority1;
    expected.Teb = kTEB1;
    expected.Stack.StartOfMemoryRange = kMemoryBase1;
    expected.Stack.Memory.DataSize = kMemorySize1;
    expected.ThreadContext.DataSize = sizeof(MinidumpContextX86);

    const MINIDUMP_MEMORY_DESCRIPTOR* observed_stack = nullptr;
    const MinidumpContextX86* observed_context = nullptr;
    ASSERT_NO_FATAL_FAILURE(
        ExpectThread(&expected,
                     &thread_list().Threads[1],
                     string_file().string(),
                     &observed_stack,
                     reinterpret_cast<const void**>(&observed_context)));

    ASSERT_NO_FATAL_FAILURE(
        ExpectMinidumpMemoryDescriptorAndContents(&expected.Stack,
                                                  observed_stack,
                                                  string_file().string(),
                                                  kMemoryValue1,
                                                  false));
    ASSERT_NO_FATAL_FAILURE(
        ExpectMinidumpContextX86(kSeed1, observed_context, false));
    ASSERT_NO_FATAL_FAILURE(ExpectMinidumpMemoryDescriptor(
        observed_stack, &memory_list().MemoryRanges[1]));
  }

  {
    SCOPED_TRACE("thread 2");

    MINIDUMP_THREAD expected = {};
    expected.ThreadId = kThreadID2;
    expected.SuspendCount = kSuspendCount2;
    expected.PriorityClass = kPriorityClass2;
    expected.Priority = kPriority2;
    expected.Teb = kTEB2;
    expected.Stack.StartOfMemoryRange = kMemoryBase2;
    expected.Stack.Memory.DataSize = kMemorySize2;
    expected.ThreadContext.DataSize = sizeof(MinidumpContextX86);

    const MINIDUMP_MEMORY_DESCRIPTOR* observed_stack = nullptr;
    const MinidumpContextX86* observed_context = nullptr;
    ASSERT_NO_FATAL_FAILURE(
        ExpectThread(&expected,
                     &thread_list().Threads[2],
                     string_file().string(),
                     &observed_stack,
                     reinterpret_cast<const void**>(&observed_context)));

    ASSERT_NO_FATAL_FAILURE(
        ExpectMinidumpMemoryDescriptorAndContents(&expected.Stack,
                                                  observed_stack,
                                                  string_file().string(),
                                                  kMemoryValue2,
                                                  true));
    ASSERT_NO_FATAL_FAILURE(
        ExpectMinidumpContextX86(kSeed2, observed_context, false));
    ASSERT_NO_FATAL_FAILURE(ExpectMinidumpMemoryDescriptor(
        observed_stack, &memory_list().MemoryRanges[2]));
  }
}

TEST_F(MinidumpThreadWriterX86Test, ThreeThreads_x86_With_Dump_Thread) {
  InitThreadWriter(true /* has_dump_thread */);
  EXPECT_EQ(thread_list().NumberOfThreads, 1u);
  EXPECT_EQ(memory_list().NumberOfMemoryRanges, 1u);
}

struct InitializeFromSnapshotX86Traits {
  using MinidumpContextType = MinidumpContextX86;
  static void InitializeCPUContext(CPUContext* context, uint32_t seed) {
    return InitializeCPUContextX86(context, seed);
  }
  static void ExpectMinidumpContext(uint32_t expect_seed,
                                    const MinidumpContextX86* observed,
                                    bool snapshot) {
    return ExpectMinidumpContextX86(expect_seed, observed, snapshot);
  }
};

struct InitializeFromSnapshotAMD64Traits {
  using MinidumpContextType = MinidumpContextAMD64;
  static void InitializeCPUContext(CPUContext* context, uint32_t seed) {
    return InitializeCPUContextX86_64(context, seed);
  }
  static void ExpectMinidumpContext(uint32_t expect_seed,
                                    const MinidumpContextAMD64* observed,
                                    bool snapshot) {
    return ExpectMinidumpContextAMD64(expect_seed, observed, snapshot);
  }
};

struct InitializeFromSnapshotNoContextTraits {
  using MinidumpContextType = MinidumpContextX86;
  static void InitializeCPUContext(CPUContext* context, uint32_t seed) {
    context->architecture = kCPUArchitectureUnknown;
  }
  static void ExpectMinidumpContext(uint32_t expect_seed,
                                    const MinidumpContextX86* observed,
                                    bool snapshot) {
    FAIL();
  }
};

template <typename Traits>
class MinidumpThreadWriterSnapshotTest : public testing::Test {
 public:
  using MinidumpContextType = typename Traits::MinidumpContextType;
  MinidumpThreadWriterSnapshotTest()
      : thread_list_(nullptr),
        memory_list_(nullptr),
        string_file_(),
        expect_threads_(),
        context_seeds_(),
        memory_values_(),
        tebs_() {}

  void InitializeFromSnapshot(bool thread_id_collision, bool has_dump_thread) {
    uint64_t thread_ids[sizeof(expect_threads_)] = {};
    expect_threads_[0].ThreadId = 1;
    expect_threads_[0].SuspendCount = 2;
    expect_threads_[0].Priority = 3;
    expect_threads_[0].Teb = 0x0123456789abcdef;
    expect_threads_[0].Stack.StartOfMemoryRange = 0x1000;
    expect_threads_[0].Stack.Memory.DataSize = 0x100;
    expect_threads_[0].ThreadContext.DataSize = sizeof(MinidumpContextType);
    memory_values_[0] = 'A';
    context_seeds_[0] = 0x80000000;
    tebs_[0].StartOfMemoryRange = expect_threads_[0].Teb;
    tebs_[0].Memory.DataSize = kTebSize;

    // The thread at index 1 has no stack.
    expect_threads_[1].ThreadId = 11;
    expect_threads_[1].SuspendCount = 12;
    expect_threads_[1].Priority = 13;
    expect_threads_[1].Teb = 0x1111111111111111;
    expect_threads_[1].ThreadContext.DataSize = sizeof(MinidumpContextType);
    context_seeds_[1] = 0x40000001;
    tebs_[1].StartOfMemoryRange = expect_threads_[1].Teb;
    tebs_[1].Memory.DataSize = kTebSize;

    expect_threads_[2].ThreadId = 21;
    expect_threads_[2].SuspendCount = 22;
    expect_threads_[2].Priority = 23;
    expect_threads_[2].Teb = 0xfedcba9876543210;
    expect_threads_[2].Stack.StartOfMemoryRange = 0x3000;
    expect_threads_[2].Stack.Memory.DataSize = 0x300;
    expect_threads_[2].ThreadContext.DataSize = sizeof(MinidumpContextType);
    memory_values_[2] = 'd';
    context_seeds_[2] = 0x20000002;
    tebs_[2].StartOfMemoryRange = expect_threads_[2].Teb;
    tebs_[2].Memory.DataSize = kTebSize;

    if (thread_id_collision) {
      thread_ids[0] = 0x0123456700000001;
      thread_ids[1] = 0x89abcdef00000001;
      thread_ids[2] = 4;
      expect_threads_[0].ThreadId = 0;
      expect_threads_[1].ThreadId = 1;
      expect_threads_[2].ThreadId = 2;
    } else {
      thread_ids[0] = 1;
      thread_ids[1] = 11;
      thread_ids[2] = 22;
      expect_threads_[0].ThreadId = static_cast<uint32_t>(thread_ids[0]);
      expect_threads_[1].ThreadId = static_cast<uint32_t>(thread_ids[1]);
      expect_threads_[2].ThreadId = static_cast<uint32_t>(thread_ids[2]);
    }

    std::vector<std::unique_ptr<TestThreadSnapshot>> thread_snapshots_owner;
    std::vector<const ThreadSnapshot*> thread_snapshots;
    for (size_t index = 0; index < base::size(expect_threads_); ++index) {
      thread_snapshots_owner.push_back(std::make_unique<TestThreadSnapshot>());
      TestThreadSnapshot* thread_snapshot = thread_snapshots_owner.back().get();

      thread_snapshot->SetThreadID(thread_ids[index]);
      thread_snapshot->SetSuspendCount(expect_threads_[index].SuspendCount);
      thread_snapshot->SetPriority(expect_threads_[index].Priority);
      thread_snapshot->SetThreadSpecificDataAddress(expect_threads_[index].Teb);

      if (expect_threads_[index].Stack.Memory.DataSize) {
        auto memory_snapshot = std::make_unique<TestMemorySnapshot>();
        memory_snapshot->SetAddress(
            expect_threads_[index].Stack.StartOfMemoryRange);
        memory_snapshot->SetSize(expect_threads_[index].Stack.Memory.DataSize);
        memory_snapshot->SetValue(memory_values_[index]);
        thread_snapshot->SetStack(std::move(memory_snapshot));
      }

      Traits::InitializeCPUContext(thread_snapshot->MutableContext(),
                                   context_seeds_[index]);

      auto teb_snapshot = std::make_unique<TestMemorySnapshot>();
      teb_snapshot->SetAddress(expect_threads_[index].Teb);
      teb_snapshot->SetSize(kTebSize);
      teb_snapshot->SetValue(static_cast<char>('t' + index));
      thread_snapshot->AddExtraMemory(std::move(teb_snapshot));

      thread_snapshots.push_back(thread_snapshot);
    }

    auto thread_list_writer =
        has_dump_thread ? std::make_unique<MinidumpThreadListWriter>(
                              expect_threads_[kDumpThreadIndex].ThreadId)
                        : std::make_unique<MinidumpThreadListWriter>();
    auto memory_list_writer = std::make_unique<MinidumpMemoryListWriter>();
    thread_list_writer->SetMemoryListWriter(memory_list_writer.get());
    MinidumpThreadIDMap thread_id_map;
    BuildMinidumpThreadIDMap(thread_snapshots, &thread_id_map);
    thread_list_writer->InitializeFromSnapshot(thread_snapshots, thread_id_map);

    MinidumpFileWriter minidump_file_writer;
    ASSERT_TRUE(minidump_file_writer.AddStream(std::move(thread_list_writer)));
    ASSERT_TRUE(minidump_file_writer.AddStream(std::move(memory_list_writer)));

    ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file_));
    ASSERT_NO_FATAL_FAILURE(GetThreadListStream(
        string_file_.string(), &thread_list_, &memory_list_));
  }

  void RunTest() {
    ASSERT_EQ(thread_list_->NumberOfThreads, 3u);
    ASSERT_EQ(memory_list_->NumberOfMemoryRanges, 5u);

    size_t memory_index = 0;
    for (size_t index = 0; index < thread_list_->NumberOfThreads; ++index) {
      SCOPED_TRACE(base::StringPrintf("index %" PRIuS, index));

      const MINIDUMP_MEMORY_DESCRIPTOR* observed_stack = nullptr;
      const MINIDUMP_MEMORY_DESCRIPTOR** observed_stack_p =
          expect_threads_[index].Stack.Memory.DataSize ? &observed_stack
                                                       : nullptr;
      const MinidumpContextType* observed_context = nullptr;
      ASSERT_NO_FATAL_FAILURE(
          ExpectThread(&expect_threads_[index],
                       &thread_list_->Threads[index],
                       string_file_.string(),
                       observed_stack_p,
                       reinterpret_cast<const void**>(&observed_context)));

      ASSERT_NO_FATAL_FAILURE(Traits::ExpectMinidumpContext(
          context_seeds_[index], observed_context, true));

      if (observed_stack_p) {
        ASSERT_NO_FATAL_FAILURE(ExpectMinidumpMemoryDescriptorAndContents(
            &expect_threads_[index].Stack,
            observed_stack,
            string_file_.string(),
            memory_values_[index],
            false));

        ASSERT_NO_FATAL_FAILURE(ExpectMinidumpMemoryDescriptor(
            observed_stack, &memory_list_->MemoryRanges[memory_index]));

        ++memory_index;
      }
    }

    for (size_t index = 0; index < thread_list_->NumberOfThreads; ++index) {
      const MINIDUMP_MEMORY_DESCRIPTOR* memory =
          &memory_list_->MemoryRanges[memory_index];
      ASSERT_NO_FATAL_FAILURE(
          ExpectMinidumpMemoryDescriptor(&tebs_[index], memory));
      std::string expected_data(kTebSize, static_cast<char>('t' + index));
      std::string observed_data(&string_file_.string()[memory->Memory.Rva],
                                memory->Memory.DataSize);
      EXPECT_EQ(observed_data, expected_data);
      ++memory_index;
    }
  }

  void VerifyDumpThread() {
    ASSERT_EQ(thread_list_->NumberOfThreads, 1u);
    ASSERT_EQ(memory_list_->NumberOfMemoryRanges, 2u);
    size_t memory_index = 0;
    const MINIDUMP_MEMORY_DESCRIPTOR* observed_stack = nullptr;
    const MINIDUMP_MEMORY_DESCRIPTOR** observed_stack_p =
        expect_threads_[kDumpThreadIndex].Stack.Memory.DataSize
            ? &observed_stack
            : nullptr;
    ASSERT_TRUE(observed_stack_p);
    const MinidumpContextType* observed_context = nullptr;
    ASSERT_NO_FATAL_FAILURE(
        ExpectThread(&expect_threads_[kDumpThreadIndex],
                     &thread_list_->Threads[0],
                     string_file_.string(),
                     observed_stack_p,
                     reinterpret_cast<const void**>(&observed_context)));

    ASSERT_NO_FATAL_FAILURE(Traits::ExpectMinidumpContext(
        context_seeds_[kDumpThreadIndex], observed_context, true));

    ASSERT_NO_FATAL_FAILURE(ExpectMinidumpMemoryDescriptorAndContents(
        &expect_threads_[kDumpThreadIndex].Stack,
        observed_stack,
        string_file_.string(),
        memory_values_[0],
        false));

    ASSERT_NO_FATAL_FAILURE(ExpectMinidumpMemoryDescriptor(
        observed_stack, &memory_list_->MemoryRanges[memory_index++]));

    const MINIDUMP_MEMORY_DESCRIPTOR* memory =
        &memory_list_->MemoryRanges[memory_index];
    ASSERT_NO_FATAL_FAILURE(
        ExpectMinidumpMemoryDescriptor(&tebs_[kDumpThreadIndex], memory));
    std::string expected_data(kTebSize,
                              static_cast<char>('t' + kDumpThreadIndex));
    std::string observed_data(&string_file_.string()[memory->Memory.Rva],
                              memory->Memory.DataSize);
    EXPECT_EQ(observed_data, expected_data);
  }

 private:
  static constexpr size_t kDumpThreadIndex = 0;
  static constexpr size_t kTebSize = 1024;
  const MINIDUMP_THREAD_LIST* thread_list_;
  const MINIDUMP_MEMORY_LIST* memory_list_;
  StringFile string_file_;
  MINIDUMP_THREAD expect_threads_[3];
  uint32_t context_seeds_[sizeof(expect_threads_)];
  uint8_t memory_values_[sizeof(expect_threads_)];
  MINIDUMP_MEMORY_DESCRIPTOR tebs_[sizeof(expect_threads_)];
};

using IntializeFromSnapshotType =
    ::testing::Types<InitializeFromSnapshotX86Traits,
                     InitializeFromSnapshotAMD64Traits>;
TYPED_TEST_SUITE(MinidumpThreadWriterSnapshotTest, IntializeFromSnapshotType);
TYPED_TEST(MinidumpThreadWriterSnapshotTest, InitializeFromSnapshot) {
  this->InitializeFromSnapshot(false /* thread_id_collision */,
                               false /* has_dump_thread */);
  this->RunTest();
}

TYPED_TEST(MinidumpThreadWriterSnapshotTest,
           IntializeFromSnapshotTypeCollision) {
  this->InitializeFromSnapshot(true /* thread_id_collision */,
                               false /* has_dump_thread */);
  this->RunTest();
}

TYPED_TEST(MinidumpThreadWriterSnapshotTest, HasDumpThread) {
  this->InitializeFromSnapshot(false /* thread_id_collision */,
                               true /* has_dump_thread */);
  this->VerifyDumpThread();
}

TEST(MinidumpThreadWriterDeathTest, NoContext) {
  MinidumpFileWriter minidump_file_writer;
  auto thread_list_writer = std::make_unique<MinidumpThreadListWriter>();

  auto thread_writer = std::make_unique<MinidumpThreadWriter>();

  thread_list_writer->AddThread(std::move(thread_writer));
  ASSERT_TRUE(minidump_file_writer.AddStream(std::move(thread_list_writer)));

  StringFile string_file;
  ASSERT_DEATH_CHECK(minidump_file_writer.WriteEverything(&string_file),
                     "context_");
}

class MinidumpThreadWriterSnapshotNoContextTest
    : public MinidumpThreadWriterSnapshotTest<
          InitializeFromSnapshotNoContextTraits> {};
TEST_F(MinidumpThreadWriterSnapshotNoContextTest,
       InitializeFromSnapshot_NoContext) {
  ASSERT_DEATH_CHECK(InitializeFromSnapshot(false /* thread_id_collision */,
                                            false /* has_dump_thread */),
                     "context_");
}

}  // namespace
}  // namespace test
}  // namespace crashpad
