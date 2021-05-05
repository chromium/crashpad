// Copyright 2021 The Crashpad Authors. All rights reserved.
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

#include "util/ios/scoped_vm_read.h"

#include <sys/time.h>

#include "gtest/gtest.h"

namespace crashpad {
namespace test {
namespace {

TEST(ScopedVMReadTest, BasicFunctionality) {
  // bad data.
  internal::ScopedVMRead<void> vmread_null(nullptr, 0);
  ASSERT_FALSE(vmread_null.is_valid());
  internal::ScopedVMRead<void> vmread_bad(0x001, 100);
  ASSERT_FALSE(vmread_bad.is_valid());

  // array
  constexpr char read_me[] = "read me";
  internal::ScopedVMRead<char> vmread_string(read_me, strlen(read_me));
  EXPECT_TRUE(vmread_string.is_valid());
  EXPECT_STREQ(read_me, vmread_string.get());

  // struct
  timeval time_of_day;
  EXPECT_TRUE(gettimeofday(&time_of_day, nullptr) == 0);
  internal::ScopedVMRead<timeval> vmread_time(&time_of_day);
  EXPECT_TRUE(vmread_time.is_valid());
  EXPECT_EQ(time_of_day.tv_sec, vmread_time->tv_sec);
  EXPECT_EQ(time_of_day.tv_usec, vmread_time->tv_usec);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
