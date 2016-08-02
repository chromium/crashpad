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

#include "client/annotations.h"

#include "base/logging.h"
#include "gtest/gtest.h"
#include "test/gtest_death_check.h"

namespace crashpad {
namespace test {
namespace {

TEST(Annotations, Annotations) {
  Annotations dict;
  dict.SetKeyValue("key1", "value1");
  dict.SetKeyValue("key2", "value2");
  dict.SetKeyValue("key3", "value3");

  std::string v1, v2, v3;
  ASSERT_TRUE(dict.GetValueForKey("key1", &v1));
  ASSERT_TRUE(dict.GetValueForKey("key2", &v2));
  ASSERT_TRUE(dict.GetValueForKey("key3", &v3));
  EXPECT_EQ(3u, dict.GetCount());
  EXPECT_EQ(3u, dict.GetNumKeys());
  EXPECT_EQ("value1", v1);
  EXPECT_EQ("value2", v2);
  EXPECT_EQ("value3", v3);

  // Try an unknown key.
  EXPECT_FALSE(dict.GetValueForKey("key4", nullptr));

  // Remove a key.
  dict.ClearKey("key3");

  EXPECT_EQ(2u, dict.GetCount());
  EXPECT_EQ(3u, dict.GetNumKeys());

  // Make sure it's not there any more.
  EXPECT_FALSE(dict.GetValueForKey("key3", nullptr));

  // Remove by setting value to nullptr.
  dict.SetKeyValue("key2", nullptr);

  EXPECT_EQ(1u, dict.GetCount());
  EXPECT_EQ(3u, dict.GetNumKeys());

  // Make sure it's not there any more.
  EXPECT_FALSE(dict.GetValueForKey("key2", nullptr));
}

TEST(Annotations, AddRemove) {
  Annotations map;
  map.SetKeyValue("rob", "ert");
  map.SetKeyValue("mike", "pink");
  map.SetKeyValue("mark", "allays");

  EXPECT_EQ(3u, map.GetCount());
  std::string str;
  EXPECT_TRUE(map.GetValueForKey("rob", &str));
  EXPECT_EQ("ert", str);
  EXPECT_TRUE(map.GetValueForKey("mike", &str));
  EXPECT_EQ("pink", str);
  EXPECT_TRUE(map.GetValueForKey("mark", &str));
  EXPECT_EQ("allays", str);

  map.ClearKey("mike");

  EXPECT_EQ(2u, map.GetCount());
  EXPECT_FALSE(map.GetValueForKey("mike", nullptr));

  map.SetKeyValue("mark", "mal");
  EXPECT_EQ(2u, map.GetCount());
  EXPECT_TRUE(map.GetValueForKey("mark", &str));
  EXPECT_EQ("mal", str);

  map.ClearKey("mark");
  EXPECT_EQ(1u, map.GetCount());
  EXPECT_FALSE(map.GetValueForKey("mark", nullptr));
}

#if DCHECK_IS_ON()

TEST(Annotations, NullKey) {
  Annotations map;
  ASSERT_DEATH_CHECK(map.SetKeyValue(nullptr, "hello"), "key");

  map.SetKeyValue("hi", "there");
  ASSERT_DEATH_CHECK(map.GetValueForKey(nullptr, nullptr), "key");
  std::string str;
  EXPECT_TRUE(map.GetValueForKey("hi", &str));
  EXPECT_EQ("there", str);

  ASSERT_DEATH_CHECK(map.ClearKey(nullptr), "key");
  map.ClearKey("hi");
  EXPECT_EQ(0u, map.GetCount());
}

#endif


}  // namespace
}  // namespace test
}  // namespace crashpad
