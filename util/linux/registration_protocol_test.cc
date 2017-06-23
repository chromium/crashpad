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

#include "util/linux/registration_protocol.h"

#include <string>

#include "gtest/gtest.h"

namespace crashpad {
namespace test {
namespace {

void ExpectConversion(const std::string& fromString,
                      pid_t expected_pid,
                      LinuxVMAddress expected_exception_address,
                      bool expected_is_valid) {
  CrashDumpRequest request;
  EXPECT_FALSE(request.IsValid());

  ASSERT_TRUE(request.InitializeFromString(fromString));
  EXPECT_EQ(request.client_process_id, expected_pid);
  EXPECT_EQ(request.exception_information_address, expected_exception_address);
  EXPECT_EQ(request.IsValid(), expected_is_valid);
}

TEST(CrashDumpRequest, StringConversions) {
  ExpectConversion("0,0x0", 0, 0, true);
  ExpectConversion("12345,0x7fffffffffffffff", 12345, 0x7fffffffffffffff, true);
  ExpectConversion("-1,0xabc", -1, 0xabc, false);
  ExpectConversion("-2,0xabc", -2, 0xabc, false);

  CrashDumpRequest request;
  EXPECT_FALSE(request.InitializeFromString("a,0x0"));
  EXPECT_FALSE(request.InitializeFromString("999999999999999999,0x0"));
  EXPECT_FALSE(request.InitializeFromString("0,-1"));
  EXPECT_FALSE(request.InitializeFromString("0,-0x1"));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
