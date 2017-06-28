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

#include "util/linux/process_memory_range.h"

#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <memory>

#include "base/logging.h"
#include "gtest/gtest.h"
#include "util/misc/from_pointer_cast.h"

namespace crashpad {
namespace test {
namespace {

struct TestObject {
  char string1[16];
  char string2[16];
} kTestObject = {"string1", "string2"};

TEST(ProcessMemoryRange, Basic) {
  pid_t pid = getpid();
#if defined(ARCH_CPU_64_BITS)
  constexpr bool is_64_bit = true;
#else
  constexpr bool is_64_bit = false;
#endif  // ARCH_CPU_64_BITS

  ProcessMemory memory;
  ASSERT_TRUE(memory.Initialize(pid));

  ProcessMemoryRange range;
  ASSERT_TRUE(range.Initialize(
      &memory, is_64_bit, 0, std::numeric_limits<LinuxVMSize>::max()));
  EXPECT_EQ(range.Is64Bit(), is_64_bit);

  EXPECT_TRUE(range.RestrictRange(0, std::numeric_limits<LinuxVMSize>::max()));

  EXPECT_TRUE(range.RestrictRange(FromPointerCast<LinuxVMAddress>(&kTestObject),
                                  sizeof(kTestObject)));

  // Both strings are accessible from the range.
  std::string string;
  auto string1_addr = FromPointerCast<LinuxVMAddress>(kTestObject.string1);
  auto string2_addr = FromPointerCast<LinuxVMAddress>(kTestObject.string2);
  ASSERT_TRUE(range.ReadCStringSizeLimited(
      string1_addr, arraysize(kTestObject.string1), &string));
  EXPECT_STREQ(string.c_str(), kTestObject.string1);

  ASSERT_TRUE(range.ReadCStringSizeLimited(
      string2_addr, arraysize(kTestObject.string2), &string));
  EXPECT_STREQ(string.c_str(), kTestObject.string2);

  // Limiting the range removes access to string2.
  ProcessMemoryRange range2;
  ASSERT_TRUE(range2.Initialize(range));
  ASSERT_TRUE(
      range2.RestrictRange(string1_addr, arraysize(kTestObject.string1)));
  EXPECT_FALSE(range2.ReadCStringSizeLimited(
      string2_addr, arraysize(kTestObject.string2), &string));
  EXPECT_TRUE(range2.ReadCStringSizeLimited(
      string1_addr, arraysize(kTestObject.string1), &string));

  // String reads fail if the NUL terminator is outside the range.
  ASSERT_TRUE(range2.RestrictRange(string1_addr, strlen(kTestObject.string1)));
  EXPECT_FALSE(range2.ReadCStringSizeLimited(
      string1_addr, arraysize(kTestObject.string1), &string));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
