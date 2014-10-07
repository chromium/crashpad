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

#include "util/test/multiprocess_exec.h"

#include <unistd.h>

#include "base/basictypes.h"
#include "gtest/gtest.h"
#include "util/file/fd_io.h"
#include "util/test/executable_path.h"

namespace crashpad {
namespace test {
namespace {

class TestMultiprocessExec final : public MultiprocessExec {
 public:
  TestMultiprocessExec() : MultiprocessExec() {}

  ~TestMultiprocessExec() {}

 private:
  virtual void MultiprocessParent() override {
    char c = 'z';
    CheckedWriteFD(WritePipeFD(), &c, 1);

    CheckedReadFD(ReadPipeFD(), &c, 1);
    EXPECT_EQ('Z', c);
  }

  DISALLOW_COPY_AND_ASSIGN(TestMultiprocessExec);
};

TEST(MultiprocessExec, MultiprocessExec) {
  TestMultiprocessExec multiprocess_exec;
  base::FilePath test_executable = ExecutablePath();
  std::string child_test_executable =
      test_executable.value() + "_multiprocess_exec_test_child";
  multiprocess_exec.SetChildCommand(child_test_executable, NULL);
  multiprocess_exec.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
