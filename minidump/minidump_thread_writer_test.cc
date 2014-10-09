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

#include <dbghelp.h>

#include "gtest/gtest.h"
#include "minidump/minidump_context_test_util.h"
#include "minidump/minidump_context_writer.h"
#include "minidump/minidump_memory_writer.h"
#include "minidump/minidump_memory_writer_test_util.h"
#include "minidump/minidump_file_writer.h"
#include "minidump/minidump_file_writer_test_util.h"
#include "util/file/string_file_writer.h"

namespace crashpad {
namespace test {
namespace {

// This returns the MINIDUMP_THREAD_LIST stream in |thread_list|. If
// |memory_list| is non-NULL, a MINIDUMP_MEMORY_LIST stream is also expected in
// |file_contents|, and that stream will be returned in |memory_list|.
void GetThreadListStream(const std::string& file_contents,
                         const MINIDUMP_THREAD_LIST** thread_list,
                         const MINIDUMP_MEMORY_LIST** memory_list) {
  const size_t kDirectoryOffset = sizeof(MINIDUMP_HEADER);
  const uint32_t kExpectedStreams = memory_list ? 2 : 1;
  const size_t kThreadListStreamOffset =
      kDirectoryOffset + kExpectedStreams * sizeof(MINIDUMP_DIRECTORY);
  const size_t kThreadsOffset =
      kThreadListStreamOffset + sizeof(MINIDUMP_THREAD_LIST);

  ASSERT_GE(file_contents.size(), kThreadsOffset);

  const MINIDUMP_HEADER* header =
      reinterpret_cast<const MINIDUMP_HEADER*>(&file_contents[0]);

  ASSERT_NO_FATAL_FAILURE(VerifyMinidumpHeader(header, kExpectedStreams, 0));

  const MINIDUMP_DIRECTORY* directory =
      reinterpret_cast<const MINIDUMP_DIRECTORY*>(
          &file_contents[kDirectoryOffset]);

  ASSERT_EQ(kMinidumpStreamTypeThreadList, directory[0].StreamType);
  ASSERT_GE(directory[0].Location.DataSize, sizeof(MINIDUMP_THREAD_LIST));
  ASSERT_EQ(kThreadListStreamOffset, directory[0].Location.Rva);

  *thread_list = reinterpret_cast<const MINIDUMP_THREAD_LIST*>(
      &file_contents[kThreadListStreamOffset]);

  ASSERT_EQ(sizeof(MINIDUMP_THREAD_LIST) +
                (*thread_list)->NumberOfThreads * sizeof(MINIDUMP_THREAD),
            directory[0].Location.DataSize);

  if (memory_list) {
    *memory_list = reinterpret_cast<const MINIDUMP_MEMORY_LIST*>(
        &file_contents[directory[1].Location.Rva]);
  }
}

TEST(MinidumpThreadWriter, EmptyThreadList) {
  MinidumpFileWriter minidump_file_writer;
  MinidumpThreadListWriter thread_list_writer;

  minidump_file_writer.AddStream(&thread_list_writer);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  ASSERT_EQ(sizeof(MINIDUMP_HEADER) + sizeof(MINIDUMP_DIRECTORY) +
                sizeof(MINIDUMP_THREAD_LIST),
            file_writer.string().size());

  const MINIDUMP_THREAD_LIST* thread_list;
  ASSERT_NO_FATAL_FAILURE(
      GetThreadListStream(file_writer.string(), &thread_list, NULL));

  EXPECT_EQ(0u, thread_list->NumberOfThreads);
}

// The MINIDUMP_THREADs |expected| and |observed| are compared against each
// other using gtest assertions. If |stack| is non-NULL, |observed| is expected
// to contain a populated MINIDUMP_MEMORY_DESCRIPTOR in its Stack field,
// otherwise, its Stack field is expected to be zeroed out. The memory
// descriptor will be placed in |stack|. |observed| must contain a populated
// ThreadContext field. The context will be recovered from |file_contents| and
// stored in |context_base|.
void ExpectThread(const MINIDUMP_THREAD* expected,
                  const MINIDUMP_THREAD* observed,
                  const std::string& file_contents,
                  const MINIDUMP_MEMORY_DESCRIPTOR** stack,
                  const void** context_base) {
  EXPECT_EQ(expected->ThreadId, observed->ThreadId);
  EXPECT_EQ(expected->SuspendCount, observed->SuspendCount);
  EXPECT_EQ(expected->PriorityClass, observed->PriorityClass);
  EXPECT_EQ(expected->Priority, observed->Priority);
  EXPECT_EQ(expected->Teb, observed->Teb);

  EXPECT_EQ(expected->Stack.StartOfMemoryRange,
            observed->Stack.StartOfMemoryRange);
  EXPECT_EQ(expected->Stack.Memory.DataSize, observed->Stack.Memory.DataSize);
  if (stack) {
    ASSERT_NE(0u, observed->Stack.Memory.DataSize);
    ASSERT_NE(0u, observed->Stack.Memory.Rva);
    ASSERT_GE(file_contents.size(),
              observed->Stack.Memory.Rva + observed->Stack.Memory.DataSize);
    *stack = &observed->Stack;
  } else {
    EXPECT_EQ(0u, observed->Stack.StartOfMemoryRange);
    EXPECT_EQ(0u, observed->Stack.Memory.DataSize);
    EXPECT_EQ(0u, observed->Stack.Memory.Rva);
  }

  EXPECT_EQ(expected->ThreadContext.DataSize, observed->ThreadContext.DataSize);
  ASSERT_NE(0u, observed->ThreadContext.DataSize);
  ASSERT_NE(0u, observed->ThreadContext.Rva);
  ASSERT_GE(file_contents.size(),
            observed->ThreadContext.Rva + expected->ThreadContext.DataSize);
  *context_base = &file_contents[observed->ThreadContext.Rva];
}

TEST(MinidumpThreadWriter, OneThread_x86_NoStack) {
  MinidumpFileWriter minidump_file_writer;
  MinidumpThreadListWriter thread_list_writer;

  const uint32_t kThreadID = 0x11111111;
  const uint32_t kSuspendCount = 1;
  const uint32_t kPriorityClass = 0x20;
  const uint32_t kPriority = 10;
  const uint64_t kTEB = 0x55555555;
  const uint32_t kSeed = 123;

  MinidumpThreadWriter thread_writer;
  thread_writer.SetThreadID(kThreadID);
  thread_writer.SetSuspendCount(kSuspendCount);
  thread_writer.SetPriorityClass(kPriorityClass);
  thread_writer.SetPriority(kPriority);
  thread_writer.SetTEB(kTEB);

  MinidumpContextX86Writer context_x86_writer;
  InitializeMinidumpContextX86(context_x86_writer.context(), kSeed);
  thread_writer.SetContext(&context_x86_writer);

  thread_list_writer.AddThread(&thread_writer);
  minidump_file_writer.AddStream(&thread_list_writer);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  ASSERT_EQ(sizeof(MINIDUMP_HEADER) + sizeof(MINIDUMP_DIRECTORY) +
                sizeof(MINIDUMP_THREAD_LIST) + 1 * sizeof(MINIDUMP_THREAD) +
                1 * sizeof(MinidumpContextX86),
            file_writer.string().size());

  const MINIDUMP_THREAD_LIST* thread_list;
  ASSERT_NO_FATAL_FAILURE(
      GetThreadListStream(file_writer.string(), &thread_list, NULL));

  EXPECT_EQ(1u, thread_list->NumberOfThreads);

  MINIDUMP_THREAD expected = {};
  expected.ThreadId = kThreadID;
  expected.SuspendCount = kSuspendCount;
  expected.PriorityClass = kPriorityClass;
  expected.Priority = kPriority;
  expected.Teb = kTEB;
  expected.ThreadContext.DataSize = sizeof(MinidumpContextX86);

  const MinidumpContextX86* observed_context;
  ASSERT_NO_FATAL_FAILURE(
      ExpectThread(&expected,
                   &thread_list->Threads[0],
                   file_writer.string(),
                   NULL,
                   reinterpret_cast<const void**>(&observed_context)));

  ASSERT_NO_FATAL_FAILURE(ExpectMinidumpContextX86(kSeed, observed_context));
}

TEST(MinidumpThreadWriter, OneThread_AMD64_Stack) {
  MinidumpFileWriter minidump_file_writer;
  MinidumpThreadListWriter thread_list_writer;

  const uint32_t kThreadID = 0x22222222;
  const uint32_t kSuspendCount = 2;
  const uint32_t kPriorityClass = 0x30;
  const uint32_t kPriority = 20;
  const uint64_t kTEB = 0x5555555555555555;
  const uint64_t kMemoryBase = 0x765432100000;
  const size_t kMemorySize = 32;
  const uint8_t kMemoryValue = 99;
  const uint32_t kSeed = 456;

  MinidumpThreadWriter thread_writer;
  thread_writer.SetThreadID(kThreadID);
  thread_writer.SetSuspendCount(kSuspendCount);
  thread_writer.SetPriorityClass(kPriorityClass);
  thread_writer.SetPriority(kPriority);
  thread_writer.SetTEB(kTEB);

  TestMinidumpMemoryWriter memory_writer(
      kMemoryBase, kMemorySize, kMemoryValue);
  thread_writer.SetStack(&memory_writer);

  MinidumpContextAMD64Writer context_amd64_writer;
  InitializeMinidumpContextAMD64(context_amd64_writer.context(), kSeed);
  thread_writer.SetContext(&context_amd64_writer);

  thread_list_writer.AddThread(&thread_writer);
  minidump_file_writer.AddStream(&thread_list_writer);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  ASSERT_EQ(sizeof(MINIDUMP_HEADER) + sizeof(MINIDUMP_DIRECTORY) +
                sizeof(MINIDUMP_THREAD_LIST) + 1 * sizeof(MINIDUMP_THREAD) +
                1 * sizeof(MinidumpContextAMD64) + kMemorySize,
            file_writer.string().size());

  const MINIDUMP_THREAD_LIST* thread_list;
  ASSERT_NO_FATAL_FAILURE(
      GetThreadListStream(file_writer.string(), &thread_list, NULL));

  EXPECT_EQ(1u, thread_list->NumberOfThreads);

  MINIDUMP_THREAD expected = {};
  expected.ThreadId = kThreadID;
  expected.SuspendCount = kSuspendCount;
  expected.PriorityClass = kPriorityClass;
  expected.Priority = kPriority;
  expected.Teb = kTEB;
  expected.Stack.StartOfMemoryRange = kMemoryBase;
  expected.Stack.Memory.DataSize = kMemorySize;
  expected.ThreadContext.DataSize = sizeof(MinidumpContextAMD64);

  const MINIDUMP_MEMORY_DESCRIPTOR* observed_stack;
  const MinidumpContextAMD64* observed_context;
  ASSERT_NO_FATAL_FAILURE(
      ExpectThread(&expected,
                   &thread_list->Threads[0],
                   file_writer.string(),
                   &observed_stack,
                   reinterpret_cast<const void**>(&observed_context)));

  ASSERT_NO_FATAL_FAILURE(
      ExpectMinidumpMemoryDescriptorAndContents(&expected.Stack,
                                                observed_stack,
                                                file_writer.string(),
                                                kMemoryValue,
                                                true));
  ASSERT_NO_FATAL_FAILURE(ExpectMinidumpContextAMD64(kSeed, observed_context));
}

TEST(MinidumpThreadWriter, ThreeThreads_x86_MemoryList) {
  MinidumpFileWriter minidump_file_writer;
  MinidumpThreadListWriter thread_list_writer;
  MinidumpMemoryListWriter memory_list_writer;
  thread_list_writer.SetMemoryListWriter(&memory_list_writer);

  const uint32_t kThreadID0 = 1111111;
  const uint32_t kSuspendCount0 = 111111;
  const uint32_t kPriorityClass0 = 11111;
  const uint32_t kPriority0 = 1111;
  const uint64_t kTEB0 = 111;
  const uint64_t kMemoryBase0 = 0x1110;
  const size_t kMemorySize0 = 16;
  const uint8_t kMemoryValue0 = 11;
  const uint32_t kSeed0 = 1;

  MinidumpThreadWriter thread_writer_0;
  thread_writer_0.SetThreadID(kThreadID0);
  thread_writer_0.SetSuspendCount(kSuspendCount0);
  thread_writer_0.SetPriorityClass(kPriorityClass0);
  thread_writer_0.SetPriority(kPriority0);
  thread_writer_0.SetTEB(kTEB0);

  TestMinidumpMemoryWriter memory_writer_0(
      kMemoryBase0, kMemorySize0, kMemoryValue0);
  thread_writer_0.SetStack(&memory_writer_0);

  MinidumpContextX86Writer context_x86_writer_0;
  InitializeMinidumpContextX86(context_x86_writer_0.context(), kSeed0);
  thread_writer_0.SetContext(&context_x86_writer_0);

  thread_list_writer.AddThread(&thread_writer_0);

  const uint32_t kThreadID1 = 2222222;
  const uint32_t kSuspendCount1 = 222222;
  const uint32_t kPriorityClass1 = 22222;
  const uint32_t kPriority1 = 2222;
  const uint64_t kTEB1 = 222;
  const uint64_t kMemoryBase1 = 0x2220;
  const size_t kMemorySize1 = 32;
  const uint8_t kMemoryValue1 = 22;
  const uint32_t kSeed1 = 2;

  MinidumpThreadWriter thread_writer_1;
  thread_writer_1.SetThreadID(kThreadID1);
  thread_writer_1.SetSuspendCount(kSuspendCount1);
  thread_writer_1.SetPriorityClass(kPriorityClass1);
  thread_writer_1.SetPriority(kPriority1);
  thread_writer_1.SetTEB(kTEB1);

  TestMinidumpMemoryWriter memory_writer_1(
      kMemoryBase1, kMemorySize1, kMemoryValue1);
  thread_writer_1.SetStack(&memory_writer_1);

  MinidumpContextX86Writer context_x86_writer_1;
  InitializeMinidumpContextX86(context_x86_writer_1.context(), kSeed1);
  thread_writer_1.SetContext(&context_x86_writer_1);

  thread_list_writer.AddThread(&thread_writer_1);

  const uint32_t kThreadID2 = 3333333;
  const uint32_t kSuspendCount2 = 333333;
  const uint32_t kPriorityClass2 = 33333;
  const uint32_t kPriority2 = 3333;
  const uint64_t kTEB2 = 333;
  const uint64_t kMemoryBase2 = 0x3330;
  const size_t kMemorySize2 = 48;
  const uint8_t kMemoryValue2 = 33;
  const uint32_t kSeed2 = 3;

  MinidumpThreadWriter thread_writer_2;
  thread_writer_2.SetThreadID(kThreadID2);
  thread_writer_2.SetSuspendCount(kSuspendCount2);
  thread_writer_2.SetPriorityClass(kPriorityClass2);
  thread_writer_2.SetPriority(kPriority2);
  thread_writer_2.SetTEB(kTEB2);

  TestMinidumpMemoryWriter memory_writer_2(
      kMemoryBase2, kMemorySize2, kMemoryValue2);
  thread_writer_2.SetStack(&memory_writer_2);

  MinidumpContextX86Writer context_x86_writer_2;
  InitializeMinidumpContextX86(context_x86_writer_2.context(), kSeed2);
  thread_writer_2.SetContext(&context_x86_writer_2);

  thread_list_writer.AddThread(&thread_writer_2);

  minidump_file_writer.AddStream(&thread_list_writer);
  minidump_file_writer.AddStream(&memory_list_writer);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&file_writer));

  ASSERT_EQ(sizeof(MINIDUMP_HEADER) + 2 * sizeof(MINIDUMP_DIRECTORY) +
                sizeof(MINIDUMP_THREAD_LIST) + 3 * sizeof(MINIDUMP_THREAD) +
                sizeof(MINIDUMP_MEMORY_LIST) +
                3 * sizeof(MINIDUMP_MEMORY_DESCRIPTOR) +
                3 * sizeof(MinidumpContextX86) + kMemorySize0 + kMemorySize1 +
                kMemorySize2 + 12,  // 12 for alignment
            file_writer.string().size());

  const MINIDUMP_THREAD_LIST* thread_list;
  const MINIDUMP_MEMORY_LIST* memory_list;
  ASSERT_NO_FATAL_FAILURE(
      GetThreadListStream(file_writer.string(), &thread_list, &memory_list));

  EXPECT_EQ(3u, thread_list->NumberOfThreads);
  EXPECT_EQ(3u, memory_list->NumberOfMemoryRanges);

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

    const MINIDUMP_MEMORY_DESCRIPTOR* observed_stack;
    const MinidumpContextX86* observed_context;
    ASSERT_NO_FATAL_FAILURE(
        ExpectThread(&expected,
                     &thread_list->Threads[0],
                     file_writer.string(),
                     &observed_stack,
                     reinterpret_cast<const void**>(&observed_context)));

    ASSERT_NO_FATAL_FAILURE(
        ExpectMinidumpMemoryDescriptorAndContents(&expected.Stack,
                                                  observed_stack,
                                                  file_writer.string(),
                                                  kMemoryValue0,
                                                  false));
    ASSERT_NO_FATAL_FAILURE(ExpectMinidumpContextX86(kSeed0, observed_context));
    ASSERT_NO_FATAL_FAILURE(ExpectMinidumpMemoryDescriptor(
        observed_stack, &memory_list->MemoryRanges[0]));
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

    const MINIDUMP_MEMORY_DESCRIPTOR* observed_stack;
    const MinidumpContextX86* observed_context;
    ASSERT_NO_FATAL_FAILURE(
        ExpectThread(&expected,
                     &thread_list->Threads[1],
                     file_writer.string(),
                     &observed_stack,
                     reinterpret_cast<const void**>(&observed_context)));

    ASSERT_NO_FATAL_FAILURE(
        ExpectMinidumpMemoryDescriptorAndContents(&expected.Stack,
                                                  observed_stack,
                                                  file_writer.string(),
                                                  kMemoryValue1,
                                                  false));
    ASSERT_NO_FATAL_FAILURE(ExpectMinidumpContextX86(kSeed1, observed_context));
    ASSERT_NO_FATAL_FAILURE(ExpectMinidumpMemoryDescriptor(
        observed_stack, &memory_list->MemoryRanges[1]));
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

    const MINIDUMP_MEMORY_DESCRIPTOR* observed_stack;
    const MinidumpContextX86* observed_context;
    ASSERT_NO_FATAL_FAILURE(
        ExpectThread(&expected,
                     &thread_list->Threads[2],
                     file_writer.string(),
                     &observed_stack,
                     reinterpret_cast<const void**>(&observed_context)));

    ASSERT_NO_FATAL_FAILURE(
        ExpectMinidumpMemoryDescriptorAndContents(&expected.Stack,
                                                  observed_stack,
                                                  file_writer.string(),
                                                  kMemoryValue2,
                                                  true));
    ASSERT_NO_FATAL_FAILURE(ExpectMinidumpContextX86(kSeed2, observed_context));
    ASSERT_NO_FATAL_FAILURE(ExpectMinidumpMemoryDescriptor(
        observed_stack, &memory_list->MemoryRanges[2]));
  }
}

TEST(MinidumpThreadWriterDeathTest, NoContext) {
  MinidumpFileWriter minidump_file_writer;
  MinidumpThreadListWriter thread_list_writer;

  MinidumpThreadWriter thread_writer;

  thread_list_writer.AddThread(&thread_writer);
  minidump_file_writer.AddStream(&thread_list_writer);

  StringFileWriter file_writer;
  ASSERT_DEATH(minidump_file_writer.WriteEverything(&file_writer), "context_");
}

}  // namespace
}  // namespace test
}  // namespace crashpad
