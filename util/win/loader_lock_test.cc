// Copyright 2019 The Crashpad Authors. All rights reserved.
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

#include "util/win/loader_lock.h"

#include "build/build_config.h"
#include "gtest/gtest.h"
#include "util/win/get_function.h"

extern "C" bool LoaderLockDetected();

namespace crashpad {
namespace test {
namespace {

#ifdef ARCH_CPU_X86_FAMILY
TEST(LoaderLock, Detected) {
  EXPECT_FALSE(IsThreadInLoaderLock());
  auto* loader_lock_detected =
      GET_FUNCTION(L"loader_lock_test_dll.dll", LoaderLockDetected);
  ASSERT_NE(loader_lock_detected, nullptr);
  EXPECT_TRUE(loader_lock_detected());
}
#endif  // ARCH_CPU_X86_FAMILY

}  // namespace
}  // namespace test
}  // namespace crashpad
