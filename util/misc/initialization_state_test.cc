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

#include "util/misc/initialization_state.h"

#include "gtest/gtest.h"

namespace crashpad {
namespace test {
namespace {

TEST(InitializationState, InitializationState) {
  InitializationState* initialization_state_pointer;
  {
    InitializationState initialization_state;
    initialization_state_pointer = &initialization_state;

    EXPECT_TRUE(initialization_state.is_uninitialized());
    EXPECT_FALSE(initialization_state.is_valid());

    initialization_state.set_invalid();

    EXPECT_FALSE(initialization_state.is_uninitialized());
    EXPECT_FALSE(initialization_state.is_valid());

    initialization_state.set_valid();

    EXPECT_FALSE(initialization_state.is_uninitialized());
    EXPECT_TRUE(initialization_state.is_valid());
  }

  // initialization_state_pointer points to something that no longer exists.
  // This portion of the test is intended to check that after an
  // InitializationState object goes out of scope, it will not be considered
  // valid on a use-after-free, assuming that nothing else was written to its
  // former home in memory.
  //
  // This portion of the test is technically not valid C++, but it exists to
  // test that the behavior is as desired when other code uses the language
  // improperly.
  EXPECT_FALSE(initialization_state_pointer->is_uninitialized());
  EXPECT_FALSE(initialization_state_pointer->is_valid());
}

}  // namespace
}  // namespace test
}  // namespace crashpad
