// Copyright 2015 The Crashpad Authors. All rights reserved.
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

#include "util/string/split_string.h"

#include "gtest/gtest.h"

namespace crashpad {
namespace test {
namespace {

TEST(SplitString, SplitString) {
  std::string left;
  std::string right;

  EXPECT_FALSE(SplitString("", '=', &left, &right));
  EXPECT_FALSE(SplitString("no equals", '=', &left, &right));
  EXPECT_FALSE(SplitString("=", '=', &left, &right));
  EXPECT_FALSE(SplitString("=beginequals", '=', &left, &right));

  ASSERT_TRUE(SplitString("a=b", '=', &left, &right));
  EXPECT_EQ("a", left);
  EXPECT_EQ("b", right);

  ASSERT_TRUE(SplitString("EndsEquals=", '=', &left, &right));
  EXPECT_EQ("EndsEquals", left);
  EXPECT_TRUE(right.empty());

  ASSERT_TRUE(SplitString("key=VALUE", '=', &left, &right));
  EXPECT_EQ("key", left);
  EXPECT_EQ("VALUE", right);

  EXPECT_FALSE(SplitString("a=b", '|', &left, &right));

  ASSERT_TRUE(SplitString("ls | less", '|', &left, &right));
  EXPECT_EQ("ls ", left);
  EXPECT_EQ(" less", right);

  ASSERT_TRUE(SplitString("when in", ' ', &left, &right));
  EXPECT_EQ("when", left);
  EXPECT_EQ("in", right);

  ASSERT_TRUE(SplitString("zoo", 'o', &left, &right));
  EXPECT_EQ("z", left);
  EXPECT_EQ("o", right);

  ASSERT_FALSE(SplitString("ooze", 'o', &left, &right));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
