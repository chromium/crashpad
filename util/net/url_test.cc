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

#include "util/net/url.h"

#include "gtest/gtest.h"

namespace crashpad {
namespace test {
namespace {

TEST(URLEncode, Empty) {
  EXPECT_EQ(URLEncode(""), "");
}

TEST(URLEncode, ReservedCharacters) {
  EXPECT_EQ(URLEncode(" !#$&'()*+,/:;=?@[]"),
            "%20%21%23%24%26%27%28%29%2A%2B%2C%2F%3A%3B%3D%3F%40%5B%5D");
}

TEST(URLEncode, UnreservedCharacters) {
  EXPECT_EQ(URLEncode("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"),
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
  EXPECT_EQ(URLEncode("0123456789-_.~"), "0123456789-_.~");
}

TEST(URLEncode, SimpleAddress) {
  EXPECT_EQ(
      URLEncode("http://some.address.com/page.html?arg1=value&arg2=value"),
      "http%3A%2F%2Fsome.address.com%2Fpage.html%3Farg1%3Dvalue%26arg2%"
      "3Dvalue");
}

}  // namespace
}  // namespace test
}  // namespace crashpad
