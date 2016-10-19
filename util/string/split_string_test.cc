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

TEST(SplitString, SplitStringFirst) {
  std::string left;
  std::string right;

  EXPECT_FALSE(SplitStringFirst("", '=', &left, &right));
  EXPECT_FALSE(SplitStringFirst("no equals", '=', &left, &right));
  EXPECT_FALSE(SplitStringFirst("=", '=', &left, &right));
  EXPECT_FALSE(SplitStringFirst("=beginequals", '=', &left, &right));

  ASSERT_TRUE(SplitStringFirst("a=b", '=', &left, &right));
  EXPECT_EQ("a", left);
  EXPECT_EQ("b", right);

  ASSERT_TRUE(SplitStringFirst("EndsEquals=", '=', &left, &right));
  EXPECT_EQ("EndsEquals", left);
  EXPECT_TRUE(right.empty());

  ASSERT_TRUE(SplitStringFirst("key=VALUE", '=', &left, &right));
  EXPECT_EQ("key", left);
  EXPECT_EQ("VALUE", right);

  EXPECT_FALSE(SplitStringFirst("a=b", '|', &left, &right));

  ASSERT_TRUE(SplitStringFirst("ls | less", '|', &left, &right));
  EXPECT_EQ("ls ", left);
  EXPECT_EQ(" less", right);

  ASSERT_TRUE(SplitStringFirst("when in", ' ', &left, &right));
  EXPECT_EQ("when", left);
  EXPECT_EQ("in", right);

  ASSERT_TRUE(SplitStringFirst("zoo", 'o', &left, &right));
  EXPECT_EQ("z", left);
  EXPECT_EQ("o", right);

  ASSERT_FALSE(SplitStringFirst("ooze", 'o', &left, &right));
}

TEST(SplitString, SplitString) {
  std::vector<std::string> parts;

  parts = SplitString("", '.');
  EXPECT_EQ(0u, parts.size());

  parts = SplitString(".", '.');
  ASSERT_EQ(2u, parts.size());
  EXPECT_EQ("", parts[0]);
  EXPECT_EQ("", parts[1]);

  parts = SplitString("a,b", ',');
  ASSERT_EQ(2u, parts.size());
  EXPECT_EQ("a", parts[0]);
  EXPECT_EQ("b", parts[1]);

  parts = SplitString("zoo", 'o');
  ASSERT_EQ(3u, parts.size());
  EXPECT_EQ("z", parts[0]);
  EXPECT_EQ("", parts[1]);
  EXPECT_EQ("", parts[2]);

  parts = SplitString("0x100,0x200,0x300,0x400,0x500,0x600", ',');
  ASSERT_EQ(6u, parts.size());
  EXPECT_EQ("0x100", parts[0]);
  EXPECT_EQ("0x200", parts[1]);
  EXPECT_EQ("0x300", parts[2]);
  EXPECT_EQ("0x400", parts[3]);
  EXPECT_EQ("0x500", parts[4]);
  EXPECT_EQ("0x600", parts[5]);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
