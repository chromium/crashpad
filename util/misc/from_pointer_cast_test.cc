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

#include "util/misc/from_pointer_cast.h"

#include <sys/types.h>

#include "gtest/gtest.h"

namespace crashpad {
namespace test {
namespace {

TEST(FromPointerCast, ToSigned) {
  EXPECT_EQ(FromPointerCast<int64_t>(nullptr), 0);
  EXPECT_EQ(FromPointerCast<int64_t>(reinterpret_cast<void*>(1)), 1);
  EXPECT_EQ(FromPointerCast<int64_t>(reinterpret_cast<void*>(-1)), -1);
  EXPECT_EQ(FromPointerCast<int64_t>(
                reinterpret_cast<void*>(std::numeric_limits<uintptr_t>::max())),
            static_cast<intptr_t>(std::numeric_limits<uintptr_t>::max()));
  EXPECT_EQ(FromPointerCast<int64_t>(
                reinterpret_cast<void*>(std::numeric_limits<intptr_t>::min())),
            std::numeric_limits<intptr_t>::min());
  EXPECT_EQ(FromPointerCast<int64_t>(
                reinterpret_cast<void*>(std::numeric_limits<intptr_t>::max())),
            std::numeric_limits<intptr_t>::max());
}

TEST(FromPointerCast, ToUnsigned) {
  EXPECT_EQ(FromPointerCast<uint64_t>(nullptr), 0u);
  EXPECT_EQ(FromPointerCast<uint64_t>(reinterpret_cast<void*>(1)), 1u);
  EXPECT_EQ(FromPointerCast<uint64_t>(reinterpret_cast<void*>(-1)),
            static_cast<uintptr_t>(-1));
  EXPECT_EQ(FromPointerCast<uint64_t>(
                reinterpret_cast<void*>(std::numeric_limits<uintptr_t>::max())),
            std::numeric_limits<uintptr_t>::max());
  EXPECT_EQ(FromPointerCast<uint64_t>(
                reinterpret_cast<void*>(std::numeric_limits<intptr_t>::min())),
            static_cast<uintptr_t>(std::numeric_limits<intptr_t>::min()));
  EXPECT_EQ(FromPointerCast<uint64_t>(
                reinterpret_cast<void*>(std::numeric_limits<intptr_t>::max())),
            static_cast<uintptr_t>(std::numeric_limits<intptr_t>::max()));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
