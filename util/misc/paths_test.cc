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

#include "util/misc/paths.h"

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "gtest/gtest.h"

namespace crashpad {
namespace test {
namespace {

TEST(Paths, Executable) {
  base::FilePath executable_path;
  ASSERT_TRUE(Paths::Executable(&executable_path));
  const base::FilePath executable_name(executable_path.BaseName());
#if defined(OS_WIN)
  EXPECT_EQ(executable_name.value(),
            FILE_PATH_LITERAL("crashpad_util_test.exe"));
#else
  EXPECT_EQ(executable_name.value(), "crashpad_util_test");
#endif  // OS_WIN
}

}  // namespace
}  // namespace test
}  // namespace crashpad
