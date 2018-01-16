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

#include "snapshot/fuchsia/process_reader.h"

#include <zircon/syscalls.h>

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "gtest/gtest.h"

namespace crashpad {
namespace test {
namespace {

TEST(ProcessReader, SelfBasic) {
  ProcessReader process_reader;
  ASSERT_TRUE(process_reader.Initialize(zx_process_self()));

  static constexpr char kTestMemory[] = "Some test memory";
  char buffer[arraysize(kTestMemory)];
  ASSERT_TRUE(process_reader.Memory()->Read(
      reinterpret_cast<zx_vaddr_t>(kTestMemory), sizeof(kTestMemory), &buffer));
  EXPECT_STREQ(kTestMemory, buffer);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
