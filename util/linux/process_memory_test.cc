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

#include "util/linux/process_memory.h"

#include <string.h>
#include <unistd.h>

#include <memory>

#include "gtest/gtest.h"
#include "test/errors.h"

namespace crashpad {
namespace test {
namespace {

pid_t ForkChild() {
  pid_t pid = fork();
  if (pid == 0) {
    raise(SIGSTOP);
    _exit(0);
  }
  return pid;
}

TEST(ProcessMemory, ReadForked) {
  const size_t kSize = 4 * PAGE_SIZE;
  std::unique_ptr<char[]> region(new char[kSize]);
  ASSERT_NE(nullptr, region.get());
  LinuxVMAddress address = reinterpret_cast<LinuxVMAddress>(region.get());

  for (size_t index = 0; index < kSize; ++index) {
    region[index] = index % 256;
  }

  pid_t pid = ForkChild();
  ASSERT_GE(pid, 0) << ErrnoMessage("fork");
  ProcessMemory memory(pid);

  std::unique_ptr<char[]> result(new char[kSize]);
  ASSERT_NE(nullptr, region.get());

  // Ensure that the entire region can be read.
  ASSERT_TRUE(memory.Read(address, kSize, result.get()));
  EXPECT_EQ(0, memcmp(region.get(), result.get(), kSize));

  // Ensure that a read of length 0 succeeds and doesn’t touch the result.
  memset(result.get(), '\0', kSize);
  ASSERT_TRUE(memory.Read(address, 0, result.get()));
  for (size_t i = 0; i < kSize; ++i) {
    EXPECT_EQ(0, result[i]);
  }

  // Ensure that a read starting at an unaligned address works.
  ASSERT_TRUE(memory.Read(address + 1, kSize - 1, result.get()));
  EXPECT_EQ(0, memcmp(region.get() + 1, result.get(), kSize - 1));

  // Ensure that a read ending at an unaligned address works.
  ASSERT_TRUE(memory.Read(address, kSize - 1, result.get()));
  EXPECT_EQ(0, memcmp(region.get(), result.get(), kSize - 1));

  // Ensure that a read starting and ending at unaligned addresses works.
  ASSERT_TRUE(memory.Read(address + 1, kSize - 2, result.get()));
  EXPECT_EQ(0, memcmp(region.get() + 1, result.get(), kSize - 2));

  // Ensure that a read of exactly one page works.
  ASSERT_TRUE(memory.Read(address + PAGE_SIZE, PAGE_SIZE, result.get()));
  EXPECT_EQ(0, memcmp(region.get() + PAGE_SIZE, result.get(), PAGE_SIZE));

  // Ensure that reading exactly a single byte works.
  result[1] = 'J';
  ASSERT_TRUE(memory.Read(address + 2, 1, result.get()));
  EXPECT_EQ(region[2], result[0]);
  EXPECT_EQ('J', result[1]);

  kill(pid, SIGKILL);
}

bool ReadCString(const ProcessMemory& memory,
                 const char* pointer,
                 std::string* result) {
  return memory.ReadCString(reinterpret_cast<LinuxVMAddress>(pointer), result);
}

TEST(ProcessMemory, ReadCStringForked) {
  const char kConstCharEmpty[] = "";
  const char kConstCharShort[] = "A short const char[]";
  static const char kStaticConstCharEmpty[] = "";
  static const char kStaticConstCharShort[] = "A short static const char[]";
  std::string string_long;
  const size_t kStringLongSize = 4 * 4096;
  for (size_t index = 0; index < kStringLongSize; ++index) {
    string_long.push_back((index % 255) + 1);
  }
  EXPECT_EQ(kStringLongSize, string_long.size());

  pid_t pid = ForkChild();
  ASSERT_GE(pid, 0) << ErrnoMessage("fork");
  ProcessMemory memory(pid);

  std::string result;

  EXPECT_TRUE(ReadCString(memory, kConstCharEmpty, &result));
  EXPECT_EQ(kConstCharEmpty, result);

  EXPECT_TRUE(ReadCString(memory, kConstCharShort, &result));
  EXPECT_EQ(kConstCharShort, result);

  EXPECT_TRUE(ReadCString(memory, kStaticConstCharEmpty, &result));
  EXPECT_EQ(kStaticConstCharEmpty, result);

  EXPECT_TRUE(ReadCString(memory, kStaticConstCharShort, &result));
  EXPECT_EQ(kStaticConstCharShort, result);

  EXPECT_TRUE(ReadCString(memory, string_long.c_str(), &result));
  EXPECT_EQ(string_long, result);
}

bool ReadCStringSizeLimited(const ProcessMemory& memory,
                            const char* pointer,
                            size_t size,
                            std::string* result) {
  return memory.ReadCStringSizeLimited(
      reinterpret_cast<LinuxVMAddress>(pointer), size, result);
}

TEST(ProcessMemory, ReadCStringSizeLimited) {
  const char kConstCharEmpty[] = "";
  const char kConstCharShort[] = "A short const char[]";
  static const char kStaticConstCharEmpty[] = "";
  static const char kStaticConstCharShort[] = "A short static const char[]";
  std::string string_long;
  const size_t kStringLongSize = 4 * 4096;
  for (size_t index = 0; index < kStringLongSize; ++index) {
    string_long.push_back((index % 255) + 1);
  }
  EXPECT_EQ(kStringLongSize, string_long.size());

  pid_t pid = ForkChild();
  ASSERT_GE(pid, 0) << ErrnoMessage("fork");
  ProcessMemory memory(pid);

  std::string result;

  EXPECT_TRUE(ReadCStringSizeLimited(
      memory, kConstCharEmpty, arraysize(kConstCharEmpty), &result));
  EXPECT_EQ(kConstCharEmpty, result);

  EXPECT_TRUE(ReadCStringSizeLimited(
      memory, kConstCharShort, arraysize(kConstCharShort), &result));
  EXPECT_EQ(kConstCharShort, result);
  EXPECT_FALSE(ReadCStringSizeLimited(
      memory, kConstCharShort, arraysize(kConstCharShort) - 1, &result));

  EXPECT_TRUE(ReadCStringSizeLimited(memory,
                                     kStaticConstCharEmpty,
                                     arraysize(kStaticConstCharEmpty),
                                     &result));
  EXPECT_EQ(kStaticConstCharEmpty, result);

  EXPECT_TRUE(ReadCStringSizeLimited(memory,
                                     kStaticConstCharShort,
                                     arraysize(kStaticConstCharShort),
                                     &result));
  EXPECT_EQ(kStaticConstCharShort, result);
  EXPECT_FALSE(ReadCStringSizeLimited(memory,
                                      kStaticConstCharShort,
                                      arraysize(kStaticConstCharShort) - 1,
                                      &result));

  EXPECT_TRUE(ReadCStringSizeLimited(
      memory, string_long.c_str(), string_long.size() + 1, &result));
  EXPECT_EQ(string_long, result);
  EXPECT_FALSE(ReadCStringSizeLimited(
      memory, string_long.c_str(), string_long.size(), &result));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
