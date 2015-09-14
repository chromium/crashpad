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

#include "util/numeric/in_range_cast.h"

#include <stdint.h>

#include <limits>

#include "gtest/gtest.h"
#include "util/misc/implicit_cast.h"

namespace crashpad {
namespace test {
namespace {

const int32_t kInt32Min = std::numeric_limits<int32_t>::min();
const int64_t kInt64Min = std::numeric_limits<int64_t>::min();

TEST(InRangeCast, Uint32) {
  EXPECT_EQ(0u, InRangeCast<uint32_t>(0, 1));
  EXPECT_EQ(1u, InRangeCast<uint32_t>(1, 1));
  EXPECT_EQ(2u, InRangeCast<uint32_t>(2, 1));
  EXPECT_EQ(0u, InRangeCast<uint32_t>(-1, 0));
  EXPECT_EQ(1u, InRangeCast<uint32_t>(-1, 1));
  EXPECT_EQ(2u, InRangeCast<uint32_t>(-1, 2));
  EXPECT_EQ(0xffffffffu, InRangeCast<uint32_t>(0xffffffffu, 1));
  EXPECT_EQ(0xffffffffu, InRangeCast<uint32_t>(UINT64_C(0xffffffff), 1));
  EXPECT_EQ(1u, InRangeCast<uint32_t>(UINT64_C(0x100000000), 1));
  EXPECT_EQ(1u, InRangeCast<uint32_t>(UINT64_C(0x100000001), 1));
  EXPECT_EQ(1u, InRangeCast<uint32_t>(kInt32Min, 1));
  EXPECT_EQ(1u, InRangeCast<uint32_t>(kInt64Min, 1));
  EXPECT_EQ(0xffffffffu, InRangeCast<uint32_t>(-1, 0xffffffffu));
}

TEST(InRangeCast, Int32) {
  EXPECT_EQ(0, InRangeCast<int32_t>(0, 1));
  EXPECT_EQ(1, InRangeCast<int32_t>(1, 1));
  EXPECT_EQ(2, InRangeCast<int32_t>(2, 1));
  EXPECT_EQ(-1, InRangeCast<int32_t>(-1, 1));
  EXPECT_EQ(0x7fffffff, InRangeCast<int32_t>(0x7fffffff, 1));
  EXPECT_EQ(0x7fffffff, InRangeCast<int32_t>(0x7fffffffu, 1));
  EXPECT_EQ(1, InRangeCast<int32_t>(0x80000000u, 1));
  EXPECT_EQ(1, InRangeCast<int32_t>(0xffffffffu, 1));
  EXPECT_EQ(1, InRangeCast<int32_t>(INT64_C(0x80000000), 1));
  EXPECT_EQ(1, InRangeCast<int32_t>(INT64_C(0xffffffff), 1));
  EXPECT_EQ(1, InRangeCast<int32_t>(INT64_C(0x100000000), 1));
  EXPECT_EQ(kInt32Min, InRangeCast<int32_t>(kInt32Min, 1));
  EXPECT_EQ(kInt32Min,
            InRangeCast<int32_t>(implicit_cast<int64_t>(kInt32Min), 1));
  EXPECT_EQ(1, InRangeCast<int32_t>(implicit_cast<int64_t>(kInt32Min) - 1, 1));
  EXPECT_EQ(1, InRangeCast<int32_t>(kInt64Min, 1));
  EXPECT_EQ(0, InRangeCast<int32_t>(0xffffffffu, 0));
  EXPECT_EQ(-1, InRangeCast<int32_t>(0xffffffffu, -1));
  EXPECT_EQ(kInt32Min, InRangeCast<int32_t>(0xffffffffu, kInt32Min));
  EXPECT_EQ(0x7fffffff, InRangeCast<int32_t>(0xffffffffu, 0x7fffffff));
}

TEST(InRangeCast, Uint64) {
  EXPECT_EQ(0u, InRangeCast<uint64_t>(0, 1));
  EXPECT_EQ(1u, InRangeCast<uint64_t>(1, 1));
  EXPECT_EQ(2u, InRangeCast<uint64_t>(2, 1));
  EXPECT_EQ(0u, InRangeCast<uint64_t>(-1, 0));
  EXPECT_EQ(1u, InRangeCast<uint64_t>(-1, 1));
  EXPECT_EQ(2u, InRangeCast<uint64_t>(-1, 2));
  EXPECT_EQ(0xffffffffu, InRangeCast<uint64_t>(0xffffffffu, 1));
  EXPECT_EQ(0xffffffffu, InRangeCast<uint64_t>(UINT64_C(0xffffffff), 1));
  EXPECT_EQ(UINT64_C(0x100000000),
            InRangeCast<uint64_t>(UINT64_C(0x100000000), 1));
  EXPECT_EQ(UINT64_C(0x100000001),
            InRangeCast<uint64_t>(UINT64_C(0x100000001), 1));
  EXPECT_EQ(1u, InRangeCast<uint64_t>(kInt32Min, 1));
  EXPECT_EQ(1u, InRangeCast<uint64_t>(INT64_C(-1), 1));
  EXPECT_EQ(1u, InRangeCast<uint64_t>(kInt64Min, 1));
  EXPECT_EQ(UINT64_C(0xffffffffffffffff),
            InRangeCast<uint64_t>(-1, UINT64_C(0xffffffffffffffff)));
}

TEST(InRangeCast, Int64) {
  EXPECT_EQ(0, InRangeCast<int64_t>(0, 1));
  EXPECT_EQ(1, InRangeCast<int64_t>(1, 1));
  EXPECT_EQ(2, InRangeCast<int64_t>(2, 1));
  EXPECT_EQ(-1, InRangeCast<int64_t>(-1, 1));
  EXPECT_EQ(0x7fffffff, InRangeCast<int64_t>(0x7fffffff, 1));
  EXPECT_EQ(0x7fffffff, InRangeCast<int64_t>(0x7fffffffu, 1));
  EXPECT_EQ(INT64_C(0x80000000), InRangeCast<int64_t>(0x80000000u, 1));
  EXPECT_EQ(INT64_C(0xffffffff), InRangeCast<int64_t>(0xffffffffu, 1));
  EXPECT_EQ(INT64_C(0x80000000), InRangeCast<int64_t>(INT64_C(0x80000000), 1));
  EXPECT_EQ(INT64_C(0xffffffff), InRangeCast<int64_t>(INT64_C(0xffffffff), 1));
  EXPECT_EQ(INT64_C(0x100000000),
            InRangeCast<int64_t>(INT64_C(0x100000000), 1));
  EXPECT_EQ(INT64_C(0x7fffffffffffffff),
            InRangeCast<int64_t>(INT64_C(0x7fffffffffffffff), 1));
  EXPECT_EQ(INT64_C(0x7fffffffffffffff),
            InRangeCast<int64_t>(UINT64_C(0x7fffffffffffffff), 1));
  EXPECT_EQ(1, InRangeCast<int64_t>(UINT64_C(0x8000000000000000), 1));
  EXPECT_EQ(1, InRangeCast<int64_t>(UINT64_C(0xffffffffffffffff), 1));
  EXPECT_EQ(kInt32Min, InRangeCast<int64_t>(kInt32Min, 1));
  EXPECT_EQ(kInt32Min,
            InRangeCast<int64_t>(implicit_cast<int64_t>(kInt32Min), 1));
  EXPECT_EQ(kInt64Min, InRangeCast<int64_t>(kInt64Min, 1));
  EXPECT_EQ(0, InRangeCast<int64_t>(UINT64_C(0xffffffffffffffff), 0));
  EXPECT_EQ(-1, InRangeCast<int64_t>(UINT64_C(0xffffffffffffffff), -1));
  EXPECT_EQ(kInt64Min,
            InRangeCast<int64_t>(UINT64_C(0xffffffffffffffff), kInt64Min));
  EXPECT_EQ(INT64_C(0x7fffffffffffffff),
            InRangeCast<int64_t>(UINT64_C(0xffffffffffffffff),
                                 INT64_C(0x7fffffffffffffff)));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
