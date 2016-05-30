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

#include "client/contiguous_string_map.h"

#include "base/logging.h"
#include "gtest/gtest.h"

namespace crashpad {
namespace test {
namespace {

TEST(ContiguousStringMap, Basic) {
  ContiguousStringMap map;
  EXPECT_EQ(0u, map.Size());
  map.Set("key1", "value1");
  EXPECT_EQ(1u, map.Size());

  EXPECT_EQ("key1", map.At(0).first.as_string());
  EXPECT_EQ("value1", map.At(0).second.as_string());

  map.Set("key1", "value2");
  EXPECT_EQ(1u, map.Size());
  EXPECT_EQ("key1", map.At(0).first.as_string());
  EXPECT_EQ("value2", map.At(0).second.as_string());

  map.Remove("key1");
  EXPECT_EQ(0u, map.Size());
}

TEST(ContiguousStringMap, StringTable) {
  ContiguousStringMap map;
  map.Set("a", "b");
  map.Set("c", "d");
  map.Set("b", "e");
  EXPECT_EQ(3u, map.Size());

  EXPECT_EQ("a", map.At(0).first.as_string());
  EXPECT_EQ("b", map.At(1).first.as_string());
  EXPECT_EQ("c", map.At(2).first.as_string());

  EXPECT_EQ("b", map.At(0).second.as_string());
  EXPECT_EQ("e", map.At(1).second.as_string());
  EXPECT_EQ("d", map.At(2).second.as_string());

  EXPECT_EQ(0, map.StringTableIndicesAt(0).first);
  EXPECT_EQ(2, map.StringTableIndicesAt(1).first);
  EXPECT_EQ(4, map.StringTableIndicesAt(2).first);

  EXPECT_EQ(2, map.StringTableIndicesAt(0).second);
  EXPECT_EQ(8, map.StringTableIndicesAt(1).second);
  EXPECT_EQ(6, map.StringTableIndicesAt(2).second);

  map.Set("a", "x");
  map.Set("b", "a");
  map.Set("c", "a");
  EXPECT_EQ(10, map.StringTableIndicesAt(0).second);
  EXPECT_EQ(0, map.StringTableIndicesAt(1).second);
  EXPECT_EQ(0, map.StringTableIndicesAt(2).second);

  // Check that the string table is collapsed to remove unused items.
  ContiguousStringMap copy(map);
  EXPECT_EQ(0, copy.StringTableIndicesAt(0).first);
  EXPECT_EQ(4, copy.StringTableIndicesAt(1).first);
  EXPECT_EQ(6, copy.StringTableIndicesAt(2).first);

  EXPECT_EQ(2, copy.StringTableIndicesAt(0).second);
  EXPECT_EQ(0, copy.StringTableIndicesAt(1).second);
  EXPECT_EQ(0, copy.StringTableIndicesAt(2).second);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
