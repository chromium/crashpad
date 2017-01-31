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

#include <stdlib.h>
#include <windows.h>

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "client/crashpad_client.h"
#include "test/paths.h"
#include "test/win/child_launcher.h"
#include "util/file/file_io.h"
#include "util/misc/random_string.h"
#include "util/win/scoped_handle.h"

namespace crashpad {
namespace test {
namespace {

int CrashMultipleChildren(int argc, wchar_t* argv[]) {
  CrashpadClient client;

  if (argc == 2) {
    if (!client.SetHandlerIPCPipe(argv[1])) {
      LOG(ERROR) << "SetHandler";
      return EXIT_FAILURE;
    }
  } else {
    fprintf(stderr, "Usage: %ls <server_pipe_name>\n", argv[0]);
    return EXIT_FAILURE;
  }

  // Launch multiple children, passing them the event they'll wait on before
  // crashing.
  base::FilePath test_executable = Paths::Executable();
  std::wstring child_test_executable =
      test_executable.DirName()
          .Append(L"multiple_crashes_test_child.exe")
          .value();
  std::wstring event_name(base::UTF8ToUTF16(RandomString()));

  ScopedKernelHANDLE crash_event(
      CreateEvent(nullptr, true, false, event_name.c_str()));
  PCHECK(crash_event.is_valid());

  std::wstring args = argv[1] + std::wstring(L" ") + event_name;

  const int kNumChildren = 500;
  std::vector<std::unique_ptr<ChildLauncher>> children;
  children.reserve(kNumChildren);
  for (int i = 0; i < kNumChildren; ++i) {
    children.emplace_back(new ChildLauncher(child_test_executable, args));
    children.back()->Start();
  }

  for (int i = 0; i < kNumChildren; ++i) {
    // Wait until ready.
    char c;
    if (!LoggingReadFile(children[i]->stdout_read_handle(), &c, sizeof(c)) ||
        c != ' ') {
      LOG(ERROR) << "failed child communication";
      return EXIT_FAILURE;
    }
  }

  Sleep(500);

  SetEvent(crash_event.get());

  Sleep(500);

  return EXIT_SUCCESS;
}

}  // namespace
}  // namespace test
}  // namespace crashpad

int wmain(int argc, wchar_t* argv[]) {
  return crashpad::test::CrashMultipleChildren(argc, argv);
}

