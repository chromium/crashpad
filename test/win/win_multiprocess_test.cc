// Copyright 2015 The Crashpad Authors. All rights reserved.
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

#include "test/win/win_multiprocess.h"

#include "base/basictypes.h"
#include "gtest/gtest.h"

namespace crashpad {
namespace test {
namespace {

class TestWinMultiprocess final : public WinMultiprocess {
 public:
  explicit TestWinMultiprocess(unsigned int exit_code)
      : WinMultiprocess(), exit_code_(exit_code) {}

  ~TestWinMultiprocess() {}

 private:
  // WinMultiprocess will have already exercised the pipes.
  void WinMultiprocessParent() override {}

  void WinMultiprocessChild() override {
    exit(exit_code_);
  }

  unsigned int exit_code_;

  DISALLOW_COPY_AND_ASSIGN(TestWinMultiprocess);
};

TEST(WinMultiprocess, WinMultiprocess) {
  TestWinMultiprocess win_multiprocess(0);
  win_multiprocess.Run();
}

TEST(WinMultiprocess, WinMultiprocessNonSuccessExitCode) {
  TestWinMultiprocess win_multiprocess(100);
  win_multiprocess.SetExpectedChildExitCode(100);
  win_multiprocess.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
