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

#include "test/main_arguments.h"

#include <string.h>

#include "gtest/gtest.h"

namespace crashpad {
namespace test {
namespace {

TEST(MainArguments, GetMainArguments) {
  // Make sure that InitializeMainArguments() has been called and that
  // GetMainArguments() provides reasonable values.

  const int* expect_argc;
  const char* const* const* expect_argv;
  GetMainArguments(&expect_argc, &expect_argv);

  ASSERT_TRUE(expect_argc);
  ASSERT_GE(*expect_argc, 1);

  ASSERT_TRUE(expect_argv);
  EXPECT_GT(strlen(*expect_argv[0]), 0u);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
