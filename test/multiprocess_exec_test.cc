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

#include "test/multiprocess_exec.h"

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "test/paths.h"
#include "util/file/file_io.h"

namespace crashpad {
namespace test {
namespace {

class TestMultiprocessExec final : public MultiprocessExec {
 public:
  TestMultiprocessExec() : MultiprocessExec() {}

  ~TestMultiprocessExec() {}

 private:
  void MultiprocessParent() override {
    // Use Logging*File() instead of Checked*File() so that the test can fail
    // gracefully with a gtest assertion if the child does not execute properly.

    char c = 'z';
    ASSERT_TRUE(LoggingWriteFile(WritePipeHandle(), &c, 1));

    ASSERT_TRUE(LoggingReadFile(ReadPipeHandle(), &c, 1));
    EXPECT_EQ('Z', c);
  }

  DISALLOW_COPY_AND_ASSIGN(TestMultiprocessExec);
};

TEST(MultiprocessExec, MultiprocessExec) {
  TestMultiprocessExec multiprocess_exec;
  base::FilePath test_executable = Paths::Executable();
#if defined(OS_POSIX)
  std::string child_test_executable = test_executable.value();
#elif defined(OS_WIN)
  std::string child_test_executable =
      base::UTF16ToUTF8(test_executable.RemoveFinalExtension().value());
#endif  // OS_POSIX
  child_test_executable += "_multiprocess_exec_test_child";
#if defined(OS_WIN)
  child_test_executable += ".exe";
#endif
  multiprocess_exec.SetChildCommand(child_test_executable, nullptr);
  multiprocess_exec.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
