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

#include "util/posix/process_util.h"

#include <crt_externs.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace crashpad {
namespace test {
namespace {

TEST(ProcessUtil, ProcessArgumentsForPID) {
  std::vector<std::string> argv;
  ASSERT_TRUE(ProcessArgumentsForPID(getpid(), &argv));

  // gtest argv processing scrambles argv, but it leaves argc and argv[0]
  // intact, so test those.

  int argc = static_cast<int>(argv.size());
  int expect_argc = *_NSGetArgc();
  EXPECT_EQ(expect_argc, argc);

  ASSERT_GE(expect_argc, 1);
  ASSERT_GE(argc, 1);

  char** expect_argv = *_NSGetArgv();
  EXPECT_EQ(std::string(expect_argv[0]), argv[0]);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
