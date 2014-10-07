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

#include "util/stdlib/strnlen.h"

#include <string.h>

#include "gtest/gtest.h"

namespace crashpad {
namespace test {
namespace {

TEST(strnlen, strnlen) {
  const char kTestBuffer[] = "abc\0d";
  ASSERT_EQ(3u, strlen(kTestBuffer));
  EXPECT_EQ(0u, crashpad::strnlen(kTestBuffer, 0));
  EXPECT_EQ(1u, crashpad::strnlen(kTestBuffer, 1));
  EXPECT_EQ(2u, crashpad::strnlen(kTestBuffer, 2));
  EXPECT_EQ(3u, crashpad::strnlen(kTestBuffer, 3));
  EXPECT_EQ(3u, crashpad::strnlen(kTestBuffer, 4));
  EXPECT_EQ(3u, crashpad::strnlen(kTestBuffer, 5));
  EXPECT_EQ(3u, crashpad::strnlen(kTestBuffer, 6));

  const char kEmptyBuffer[] = "\0";
  ASSERT_EQ(0u, strlen(kEmptyBuffer));
  EXPECT_EQ(0u, crashpad::strnlen(kEmptyBuffer, 0));
  EXPECT_EQ(0u, crashpad::strnlen(kEmptyBuffer, 1));
  EXPECT_EQ(0u, crashpad::strnlen(kEmptyBuffer, 2));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
