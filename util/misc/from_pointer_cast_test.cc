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

struct SomeType {};

template <typename T>
class FromPointerCastTest : public testing::Test {};

using FromPointerCastTestTypes = testing::Types<void*,
                                                const void*,
                                                volatile void*,
                                                const volatile void*,
                                                SomeType*,
                                                const SomeType*,
                                                volatile SomeType*,
                                                const volatile SomeType*>;

TYPED_TEST_CASE(FromPointerCastTest, FromPointerCastTestTypes);

TYPED_TEST(FromPointerCastTest, ToSigned) {
  EXPECT_EQ(FromPointerCast<int64_t>(nullptr), 0);
  EXPECT_EQ(FromPointerCast<int64_t>(reinterpret_cast<TypeParam>(1)), 1);
  EXPECT_EQ(FromPointerCast<int64_t>(reinterpret_cast<TypeParam>(-1)), -1);
  EXPECT_EQ(FromPointerCast<int64_t>(reinterpret_cast<TypeParam>(
                std::numeric_limits<uintptr_t>::max())),
            static_cast<intptr_t>(std::numeric_limits<uintptr_t>::max()));
  EXPECT_EQ(FromPointerCast<int64_t>(reinterpret_cast<TypeParam>(
                std::numeric_limits<intptr_t>::min())),
            std::numeric_limits<intptr_t>::min());
  EXPECT_EQ(FromPointerCast<int64_t>(reinterpret_cast<TypeParam>(
                std::numeric_limits<intptr_t>::max())),
            std::numeric_limits<intptr_t>::max());
}

TYPED_TEST(FromPointerCastTest, ToUnsigned) {
  EXPECT_EQ(FromPointerCast<uint64_t>(nullptr), 0u);
  EXPECT_EQ(FromPointerCast<uint64_t>(reinterpret_cast<TypeParam>(1)), 1u);
  EXPECT_EQ(FromPointerCast<uint64_t>(reinterpret_cast<TypeParam>(-1)),
            static_cast<uintptr_t>(-1));
  EXPECT_EQ(FromPointerCast<uint64_t>(reinterpret_cast<TypeParam>(
                std::numeric_limits<uintptr_t>::max())),
            std::numeric_limits<uintptr_t>::max());
  EXPECT_EQ(FromPointerCast<uint64_t>(reinterpret_cast<TypeParam>(
                std::numeric_limits<intptr_t>::min())),
            static_cast<uintptr_t>(std::numeric_limits<intptr_t>::min()));
  EXPECT_EQ(FromPointerCast<uint64_t>(reinterpret_cast<TypeParam>(
                std::numeric_limits<intptr_t>::max())),
            static_cast<uintptr_t>(std::numeric_limits<intptr_t>::max()));
}

TYPED_TEST(FromPointerCastTest, ToPointer) {
  using TypeParamPointee = typename std::remove_pointer<TypeParam>::type;
  using CVSomeType = typename std::conditional<
      std::is_const<TypeParamPointee>::value,
      typename std::conditional<std::is_volatile<TypeParamPointee>::value,
                                const volatile SomeType,
                                const SomeType>::type,
      typename std::conditional<std::is_volatile<TypeParamPointee>::value,
                                volatile SomeType,
                                SomeType>::type>::type;

  EXPECT_EQ(FromPointerCast<CVSomeType*>(nullptr),
            static_cast<CVSomeType*>(nullptr));
  EXPECT_EQ(FromPointerCast<CVSomeType*>(reinterpret_cast<TypeParam>(1)),
            reinterpret_cast<CVSomeType*>(1));
  EXPECT_EQ(FromPointerCast<CVSomeType*>(reinterpret_cast<TypeParam>(-1)),
            reinterpret_cast<CVSomeType*>(-1));
  EXPECT_EQ(
      FromPointerCast<CVSomeType*>(
          reinterpret_cast<TypeParam>(std::numeric_limits<uintptr_t>::max())),
      reinterpret_cast<CVSomeType*>(std::numeric_limits<uintptr_t>::max()));
  EXPECT_EQ(
      FromPointerCast<CVSomeType*>(
          reinterpret_cast<TypeParam>(std::numeric_limits<intptr_t>::min())),
      reinterpret_cast<CVSomeType*>(std::numeric_limits<intptr_t>::min()));
  EXPECT_EQ(
      FromPointerCast<CVSomeType*>(
          reinterpret_cast<TypeParam>(std::numeric_limits<intptr_t>::max())),
      reinterpret_cast<CVSomeType*>(std::numeric_limits<intptr_t>::max()));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
