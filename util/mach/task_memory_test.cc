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

#include "util/mach/task_memory.h"

#include <mach/mach.h>

#include <algorithm>
#include <string>

#include "base/mac/scoped_mach_vm.h"
#include "gtest/gtest.h"
#include "util/test/mac/mach_errors.h"

namespace {

using namespace crashpad;
using namespace crashpad::test;

TEST(TaskMemory, ReadSelf) {
  vm_address_t address = 0;
  const vm_size_t kSize = 4 * PAGE_SIZE;
  kern_return_t kr =
      vm_allocate(mach_task_self(), &address, kSize, VM_FLAGS_ANYWHERE);
  ASSERT_EQ(KERN_SUCCESS, kr) << MachErrorMessage(kr, "vm_allocate");
  base::mac::ScopedMachVM vm_owner(address, mach_vm_round_page(kSize));

  char* region = reinterpret_cast<char*>(address);
  for (size_t index = 0; index < kSize; ++index) {
    region[index] = (index % 256) ^ ((index >> 8) % 256);
  }

  TaskMemory memory(mach_task_self());
  std::string result(kSize, '\0');

  // Ensure that the entire region can be read.
  ASSERT_TRUE(memory.Read(address, kSize, &result[0]));
  EXPECT_EQ(0, memcmp(region, &result[0], kSize));

  // Ensure that a read of length 0 succeeds and doesn’t touch the result.
  result.assign(kSize, '\0');
  std::string zeroes = result;
  ASSERT_TRUE(memory.Read(address, 0, &result[0]));
  EXPECT_EQ(zeroes, result);

  // Ensure that a read starting at an unaligned address works.
  ASSERT_TRUE(memory.Read(address + 1, kSize - 1, &result[0]));
  EXPECT_EQ(0, memcmp(region + 1, &result[0], kSize - 1));

  // Ensure that a read ending at an unaligned address works.
  ASSERT_TRUE(memory.Read(address, kSize - 1, &result[0]));
  EXPECT_EQ(0, memcmp(region, &result[0], kSize - 1));

  // Ensure that a read starting and ending at unaligned addresses works.
  ASSERT_TRUE(memory.Read(address + 1, kSize - 2, &result[0]));
  EXPECT_EQ(0, memcmp(region + 1, &result[0], kSize - 2));

  // Ensure that a read of exactly one page works.
  ASSERT_TRUE(memory.Read(address + PAGE_SIZE, PAGE_SIZE, &result[0]));
  EXPECT_EQ(0, memcmp(region + PAGE_SIZE, &result[0], PAGE_SIZE));

  // Ensure that a read of a single byte works.
  ASSERT_TRUE(memory.Read(address + 2, 1, &result[0]));
  EXPECT_EQ(region[2], result[0]);

  // Ensure that a read of length zero works and doesn’t touch the data.
  result[0] = 'M';
  ASSERT_TRUE(memory.Read(address + 3, 0, &result[0]));
  EXPECT_EQ('M', result[0]);
}

TEST(TaskMemory, ReadSelfUnmapped) {
  vm_address_t address = 0;
  const vm_size_t kSize = 2 * PAGE_SIZE;
  kern_return_t kr =
      vm_allocate(mach_task_self(), &address, kSize, VM_FLAGS_ANYWHERE);
  ASSERT_EQ(KERN_SUCCESS, kr) << MachErrorMessage(kr, "vm_allocate");
  base::mac::ScopedMachVM vm_owner(address, mach_vm_round_page(kSize));

  char* region = reinterpret_cast<char*>(address);
  for (size_t index = 0; index < kSize; ++index) {
    // Don’t include any NUL bytes, because ReadCString stops when it encounters
    // a NUL.
    region[index] = (index % 255) + 1;
  }

  kr = vm_protect(
      mach_task_self(), address + PAGE_SIZE, PAGE_SIZE, FALSE, VM_PROT_NONE);
  ASSERT_EQ(KERN_SUCCESS, kr) << MachErrorMessage(kr, "vm_protect");

  TaskMemory memory(mach_task_self());
  std::string result(kSize, '\0');

  EXPECT_FALSE(memory.Read(address, kSize, &result[0]));
  EXPECT_FALSE(memory.Read(address + 1, kSize - 1, &result[0]));
  EXPECT_FALSE(memory.Read(address + PAGE_SIZE, 1, &result[0]));
  EXPECT_FALSE(memory.Read(address + PAGE_SIZE - 1, 2, &result[0]));
  EXPECT_TRUE(memory.Read(address, PAGE_SIZE, &result[0]));
  EXPECT_TRUE(memory.Read(address + PAGE_SIZE - 1, 1, &result[0]));

  // Repeat the test with an unmapped page instead of an unreadable one. This
  // portion of the test may be flaky in the presence of other threads, if
  // another thread maps something in the region that is deallocated here.
  kr = vm_deallocate(mach_task_self(), address + PAGE_SIZE, PAGE_SIZE);
  ASSERT_EQ(KERN_SUCCESS, kr) << MachErrorMessage(kr, "vm_deallocate");
  vm_owner.reset(address, PAGE_SIZE);

  EXPECT_FALSE(memory.Read(address, kSize, &result[0]));
  EXPECT_FALSE(memory.Read(address + 1, kSize - 1, &result[0]));
  EXPECT_FALSE(memory.Read(address + PAGE_SIZE, 1, &result[0]));
  EXPECT_FALSE(memory.Read(address + PAGE_SIZE - 1, 2, &result[0]));
  EXPECT_TRUE(memory.Read(address, PAGE_SIZE, &result[0]));
  EXPECT_TRUE(memory.Read(address + PAGE_SIZE - 1, 1, &result[0]));
}

// This function consolidates the cast from a char* to mach_vm_address_t in one
// location when reading from the current task.
bool ReadCStringSelf(TaskMemory* memory,
                     const char* pointer,
                     std::string* result) {
  return memory->ReadCString(reinterpret_cast<mach_vm_address_t>(pointer),
                             result);
}

TEST(TaskMemory, ReadCStringSelf) {
  TaskMemory memory(mach_task_self());
  std::string result;

  const char kConstCharEmpty[] = "";
  ASSERT_TRUE(ReadCStringSelf(&memory, kConstCharEmpty, &result));
  EXPECT_TRUE(result.empty());
  EXPECT_EQ(kConstCharEmpty, result);

  const char kConstCharShort[] = "A short const char[]";
  ASSERT_TRUE(ReadCStringSelf(&memory, kConstCharShort, &result));
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(kConstCharShort, result);

  static const char kStaticConstCharEmpty[] = "";
  ASSERT_TRUE(ReadCStringSelf(&memory, kStaticConstCharEmpty, &result));
  EXPECT_TRUE(result.empty());
  EXPECT_EQ(kStaticConstCharEmpty, result);

  static const char kStaticConstCharShort[] = "A short static const char[]";
  ASSERT_TRUE(ReadCStringSelf(&memory, kStaticConstCharShort, &result));
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(kStaticConstCharShort, result);

  std::string string_short("A short std::string in a function");
  ASSERT_TRUE(ReadCStringSelf(&memory, &string_short[0], &result));
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(string_short, result);

  std::string string_long;
  const size_t kStringLongSize = 4 * PAGE_SIZE;
  for (size_t index = 0; index < kStringLongSize; ++index) {
    // Don’t include any NUL bytes, because ReadCString stops when it encounters
    // a NUL.
    string_long.append(1, (index % 255) + 1);
  }
  ASSERT_EQ(kStringLongSize, string_long.size());
  ASSERT_TRUE(ReadCStringSelf(&memory, &string_long[0], &result));
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(kStringLongSize, result.size());
  EXPECT_EQ(string_long, result);
}

TEST(TaskMemory, ReadCStringSelfUnmapped) {
  vm_address_t address = 0;
  const vm_size_t kSize = 2 * PAGE_SIZE;
  kern_return_t kr =
      vm_allocate(mach_task_self(), &address, kSize, VM_FLAGS_ANYWHERE);
  ASSERT_EQ(KERN_SUCCESS, kr) << MachErrorMessage(kr, "vm_allocate");
  base::mac::ScopedMachVM vm_owner(address, mach_vm_round_page(kSize));

  char* region = reinterpret_cast<char*>(address);
  for (size_t index = 0; index < kSize; ++index) {
    // Don’t include any NUL bytes, because ReadCString stops when it encounters
    // a NUL.
    region[index] = (index % 255) + 1;
  }

  kr = vm_protect(
      mach_task_self(), address + PAGE_SIZE, PAGE_SIZE, FALSE, VM_PROT_NONE);
  ASSERT_EQ(KERN_SUCCESS, kr) << MachErrorMessage(kr, "vm_protect");

  TaskMemory memory(mach_task_self());
  std::string result;
  EXPECT_FALSE(memory.ReadCString(address, &result));

  // Make sure that if the string is NUL-terminated within the mapped memory
  // region, it can be read properly.
  char terminator_or_not = '\0';
  std::swap(region[PAGE_SIZE - 1], terminator_or_not);
  ASSERT_TRUE(memory.ReadCString(address, &result));
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(PAGE_SIZE - 1u, result.size());
  EXPECT_EQ(region, result);

  // Repeat the test with an unmapped page instead of an unreadable one. This
  // portion of the test may be flaky in the presence of other threads, if
  // another thread maps something in the region that is deallocated here.
  std::swap(region[PAGE_SIZE - 1], terminator_or_not);
  kr = vm_deallocate(mach_task_self(), address + PAGE_SIZE, PAGE_SIZE);
  ASSERT_EQ(KERN_SUCCESS, kr) << MachErrorMessage(kr, "vm_deallocate");
  vm_owner.reset(address, PAGE_SIZE);

  EXPECT_FALSE(memory.ReadCString(address, &result));

  // Clear the result before testing that the string can be read. This makes
  // sure that the result is actually filled in, because it already contains the
  // expected value from the tests above.
  result.clear();
  std::swap(region[PAGE_SIZE - 1], terminator_or_not);
  ASSERT_TRUE(memory.ReadCString(address, &result));
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(PAGE_SIZE - 1u, result.size());
  EXPECT_EQ(region, result);
}

// This function consolidates the cast from a char* to mach_vm_address_t in one
// location when reading from the current task.
bool ReadCStringSizeLimitedSelf(TaskMemory* memory,
                                const char* pointer,
                                size_t size,
                                std::string* result) {
  return memory->ReadCStringSizeLimited(
      reinterpret_cast<mach_vm_address_t>(pointer), size, result);
}

TEST(TaskMemory, ReadCStringSizeLimited_ConstCharEmpty) {
  TaskMemory memory(mach_task_self());
  std::string result;

  const char kConstCharEmpty[] = "";
  ASSERT_TRUE(ReadCStringSizeLimitedSelf(
      &memory, kConstCharEmpty, arraysize(kConstCharEmpty), &result));
  EXPECT_TRUE(result.empty());
  EXPECT_EQ(kConstCharEmpty, result);

  result.clear();
  ASSERT_TRUE(ReadCStringSizeLimitedSelf(
      &memory, kConstCharEmpty, arraysize(kConstCharEmpty) + 1, &result));
  EXPECT_TRUE(result.empty());
  EXPECT_EQ(kConstCharEmpty, result);

  result.clear();
  ASSERT_TRUE(ReadCStringSizeLimitedSelf(&memory, kConstCharEmpty, 0, &result));
  EXPECT_TRUE(result.empty());
  EXPECT_EQ(kConstCharEmpty, result);
}

TEST(TaskMemory, ReadCStringSizeLimited_ConstCharShort) {
  TaskMemory memory(mach_task_self());
  std::string result;

  const char kConstCharShort[] = "A short const char[]";
  ASSERT_TRUE(ReadCStringSizeLimitedSelf(
      &memory, kConstCharShort, arraysize(kConstCharShort), &result));
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(kConstCharShort, result);

  result.clear();
  ASSERT_TRUE(ReadCStringSizeLimitedSelf(
      &memory, kConstCharShort, arraysize(kConstCharShort) + 1, &result));
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(kConstCharShort, result);

  ASSERT_FALSE(ReadCStringSizeLimitedSelf(
      &memory, kConstCharShort, arraysize(kConstCharShort) - 1, &result));
}

TEST(TaskMemory, ReadCStringSizeLimited_StaticConstCharEmpty) {
  TaskMemory memory(mach_task_self());
  std::string result;

  static const char kStaticConstCharEmpty[] = "";
  ASSERT_TRUE(ReadCStringSizeLimitedSelf(&memory,
                                         kStaticConstCharEmpty,
                                         arraysize(kStaticConstCharEmpty),
                                         &result));
  EXPECT_TRUE(result.empty());
  EXPECT_EQ(kStaticConstCharEmpty, result);

  result.clear();
  ASSERT_TRUE(ReadCStringSizeLimitedSelf(&memory,
                                         kStaticConstCharEmpty,
                                         arraysize(kStaticConstCharEmpty) + 1,
                                         &result));
  EXPECT_TRUE(result.empty());
  EXPECT_EQ(kStaticConstCharEmpty, result);

  result.clear();
  ASSERT_TRUE(
      ReadCStringSizeLimitedSelf(&memory, kStaticConstCharEmpty, 0, &result));
  EXPECT_TRUE(result.empty());
  EXPECT_EQ(kStaticConstCharEmpty, result);
}

TEST(TaskMemory, ReadCStringSizeLimited_StaticConstCharShort) {
  TaskMemory memory(mach_task_self());
  std::string result;

  static const char kStaticConstCharShort[] = "A short static const char[]";
  ASSERT_TRUE(ReadCStringSizeLimitedSelf(&memory,
                                         kStaticConstCharShort,
                                         arraysize(kStaticConstCharShort),
                                         &result));
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(kStaticConstCharShort, result);

  result.clear();
  ASSERT_TRUE(ReadCStringSizeLimitedSelf(&memory,
                                         kStaticConstCharShort,
                                         arraysize(kStaticConstCharShort) + 1,
                                         &result));
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(kStaticConstCharShort, result);

  ASSERT_FALSE(ReadCStringSizeLimitedSelf(&memory,
                                          kStaticConstCharShort,
                                          arraysize(kStaticConstCharShort) - 1,
                                          &result));
}

TEST(TaskMemory, ReadCStringSizeLimited_StringShort) {
  TaskMemory memory(mach_task_self());
  std::string result;

  std::string string_short("A short std::string in a function");
  ASSERT_TRUE(ReadCStringSizeLimitedSelf(
      &memory, &string_short[0], string_short.size() + 1, &result));
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(string_short, result);

  result.clear();
  ASSERT_TRUE(ReadCStringSizeLimitedSelf(
      &memory, &string_short[0], string_short.size() + 2, &result));
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(string_short, result);

  ASSERT_FALSE(ReadCStringSizeLimitedSelf(
      &memory, &string_short[0], string_short.size(), &result));
}

TEST(TaskMemory, ReadCStringSizeLimited_StringLong) {
  TaskMemory memory(mach_task_self());
  std::string result;

  std::string string_long;
  const size_t kStringLongSize = 4 * PAGE_SIZE;
  for (size_t index = 0; index < kStringLongSize; ++index) {
    // Don’t include any NUL bytes, because ReadCString stops when it encounters
    // a NUL.
    string_long.append(1, (index % 255) + 1);
  }
  ASSERT_EQ(kStringLongSize, string_long.size());
  ASSERT_TRUE(ReadCStringSizeLimitedSelf(
      &memory, &string_long[0], string_long.size() + 1, &result));
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(kStringLongSize, result.size());
  EXPECT_EQ(string_long, result);

  result.clear();
  ASSERT_TRUE(ReadCStringSizeLimitedSelf(
      &memory, &string_long[0], string_long.size() + 2, &result));
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(kStringLongSize, result.size());
  EXPECT_EQ(string_long, result);

  ASSERT_FALSE(ReadCStringSizeLimitedSelf(
      &memory, &string_long[0], string_long.size(), &result));
}

}  // namespace
