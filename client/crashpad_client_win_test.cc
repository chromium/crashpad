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

#include "client/crashpad_client.h"

#include "base/files/file_path.h"
#include "gtest/gtest.h"
#include "test/paths.h"
#include "test/scoped_temp_dir.h"
#include "test/win/win_multiprocess.h"

namespace crashpad {
namespace test {
namespace {

void StartAndUseHandler() {
  ScopedTempDir temp_dir;
  base::FilePath handler_path = Paths::Executable().DirName().Append(
      FILE_PATH_LITERAL("crashpad_handler.exe"));
  CrashpadClient client;
  ASSERT_TRUE(client.StartHandler(handler_path,
                                  temp_dir.path(),
                                  "",
                                  std::map<std::string, std::string>(),
                                  std::vector<std::string>(),
                                  false));
  EXPECT_TRUE(client.UseHandler());
}

class StartWithInvalidHandles final : public WinMultiprocess {
 public:
  StartWithInvalidHandles() : WinMultiprocess() {}
  ~StartWithInvalidHandles() {}

 private:
  void WinMultiprocessParent() override {}

  void WinMultiprocessChild() override {
    HANDLE original_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE original_stderr = GetStdHandle(STD_ERROR_HANDLE);
    SetStdHandle(STD_OUTPUT_HANDLE, INVALID_HANDLE_VALUE);
    SetStdHandle(STD_ERROR_HANDLE, INVALID_HANDLE_VALUE);

    StartAndUseHandler();

    SetStdHandle(STD_OUTPUT_HANDLE, original_stdout);
    SetStdHandle(STD_ERROR_HANDLE, original_stderr);
  }
};

TEST(CrashpadClient, StartWithInvalidHandles) {
  WinMultiprocess::Run<StartWithInvalidHandles>();
}

class StartWithSameStdoutStderr final : public WinMultiprocess {
 public:
  StartWithSameStdoutStderr() : WinMultiprocess() {}
  ~StartWithSameStdoutStderr() {}

 private:
  void WinMultiprocessParent() override {}

  void WinMultiprocessChild() override {
    HANDLE original_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE original_stderr = GetStdHandle(STD_ERROR_HANDLE);
    SetStdHandle(STD_OUTPUT_HANDLE, original_stderr);

    StartAndUseHandler();

    SetStdHandle(STD_OUTPUT_HANDLE, original_stdout);
  }
};

TEST(CrashpadClient, StartWithSameStdoutStderr) {
  WinMultiprocess::Run<StartWithSameStdoutStderr>();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
