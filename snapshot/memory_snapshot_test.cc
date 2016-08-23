// Copyright 2016 The Crashpad Authors. All rights reserved.
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

#include "snapshot/memory_snapshot.h"

#include "base/macros.h"
#include "gtest/gtest.h"
#include "snapshot/test/test_memory_snapshot.h"

namespace crashpad {
namespace test {
namespace {

TEST(DetermineMergedRange, NonOverlapping) {
  TestMemorySnapshot a;
  TestMemorySnapshot b;
  a.SetAddress(0);
  a.SetSize(100);
  b.SetAddress(200);
  b.SetSize(50);
  CheckedRange<uint64_t, size_t> range(0, 0);
  EXPECT_FALSE(DetermineMergedRange(a, b, &range));
  EXPECT_FALSE(DetermineMergedRange(b, a, &range));

  a.SetSize(199);
  EXPECT_FALSE(DetermineMergedRange(a, b, &range));
}

TEST(DetermineMergedRange, Empty) {
  TestMemorySnapshot a;
  TestMemorySnapshot b;
  a.SetAddress(100);
  a.SetSize(0);
  b.SetAddress(200);
  b.SetSize(20);
  CheckedRange<uint64_t, size_t> range(0, 0);
  EXPECT_TRUE(DetermineMergedRange(a, b, &range));
  EXPECT_EQ(200, range.base());
  EXPECT_EQ(20, range.size());
  EXPECT_TRUE(DetermineMergedRange(b, a, &range));
  EXPECT_EQ(200, range.base());
  EXPECT_EQ(20, range.size());

  b.SetAddress(50);
  b.SetSize(1000);
  EXPECT_TRUE(DetermineMergedRange(a, b, &range));
  EXPECT_EQ(50, range.base());
  EXPECT_EQ(1000, range.size());
  EXPECT_TRUE(DetermineMergedRange(b, a, &range));
  EXPECT_EQ(50, range.base());
  EXPECT_EQ(1000, range.size());

  EXPECT_FALSE(DetermineMergedRange(a, a, &range));
}

TEST(DetermineMergedRange, Abutting) {
  TestMemorySnapshot a;
  TestMemorySnapshot b;
  a.SetAddress(0);
  a.SetSize(10);
  b.SetAddress(10);
  b.SetSize(20);
  CheckedRange<uint64_t, size_t> range(0, 0);
  EXPECT_TRUE(DetermineMergedRange(a, b, &range));
  EXPECT_EQ(0, range.base());
  EXPECT_EQ(30, range.size());

  EXPECT_TRUE(DetermineMergedRange(b, a, &range));
  EXPECT_EQ(0, range.base());
  EXPECT_EQ(30, range.size());
}

TEST(DetermineMergedRange, Overlapping) {
  TestMemorySnapshot a;
  TestMemorySnapshot b;
  a.SetAddress(10);
  a.SetSize(100);
  b.SetAddress(50);
  b.SetSize(100);
  CheckedRange<uint64_t, size_t> range(0, 0);
  EXPECT_TRUE(DetermineMergedRange(a, b, &range));
  EXPECT_EQ(10, range.base());
  EXPECT_EQ(140, range.size());

  b.SetAddress(5);
  b.SetSize(200);
  EXPECT_TRUE(DetermineMergedRange(a, b, &range));
  EXPECT_EQ(5, range.base());
  EXPECT_EQ(200, range.size());

  b.SetAddress(5);
  b.SetSize(50);
  EXPECT_TRUE(DetermineMergedRange(a, b, &range));
  EXPECT_EQ(5, range.base());
  EXPECT_EQ(105, range.size());
}

}  // namespace
}  // namespace test
}  // namespace crashpad
