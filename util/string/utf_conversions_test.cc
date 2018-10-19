// Copyright 2018 The Crashpad Authors. All rights reserved.
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

#include "gtest/gtest.h"

#include "base/strings/utf_string_conversions.h"

namespace crashpad {
namespace test {
namespace {

TEST(UTFStringConversion, UTF16To8) {
  const base::char16 test[] = {'s', 'c', 'e', 'n', 'i', 'c', '\0'};
  std::string output;
  EXPECT_TRUE(base::UTF16ToUTF8(test, 6, &output));
  EXPECT_EQ(output.size(), 6u);
  EXPECT_EQ(output, "scenic");
}

}  // namespace
}  // namespace test
}  // namespace crashpad
