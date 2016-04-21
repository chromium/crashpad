// Copyright 2016 The Crashpad Authors. All rights reserved.
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
#include <tlhelp32.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "client/crashpad_client.h"
#include "test/paths.h"
#include "test/win/child_launcher.h"
#include "util/file/file_io.h"
#include "util/win/scoped_handle.h"
#include "util/win/xp_compat.h"

namespace crashpad {
namespace test {
namespace {

bool CrashAndDumpTarget(const CrashpadClient& client, HANDLE process) {
  DWORD target_pid = GetProcessId(process);

  HANDLE thread_snap_raw = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (thread_snap_raw == INVALID_HANDLE_VALUE) {
    LOG(ERROR) << "CreateToolhelp32Snapshot";
    return false;
  }
  ScopedFileHANDLE thread_snap(thread_snap_raw);

  THREADENTRY32 te32;
  te32.dwSize = sizeof(THREADENTRY32);
  if (!Thread32First(thread_snap.get(), &te32)) {
    LOG(ERROR) << "Thread32First";
    return false;
  }

  int thread_count = 0;
  do {
    if (te32.th32OwnerProcessID == target_pid) {
      thread_count++;
      if (thread_count == 2) {
        // Nominate this lucky thread as our blamee, and dump it. This will be
        // "Thread1" in the child.
        ScopedKernelHANDLE thread(
            OpenThread(kXPThreadAllAccess, false, te32.th32ThreadID));
        if (!client.DumpAndCrashTargetProcess(
                process, thread.get(), 0xdeadbea7)) {
          LOG(ERROR) << "DumpAndCrashTargetProcess failed";
          return false;
        }
        return true;
      }
    }
  } while (Thread32Next(thread_snap.get(), &te32));

  return false;
}

int CrashOtherProgram(int argc, wchar_t* argv[]) {
  CrashpadClient client;

  if (argc == 2 || argc == 3) {
    if (!client.SetHandlerIPCPipe(argv[1])) {
      LOG(ERROR) << "SetHandler";
      return EXIT_FAILURE;
    }
  } else {
    fprintf(stderr, "Usage: %ls <server_pipe_name> [noexception]\n", argv[0]);
    return EXIT_FAILURE;
  }

  if (!client.UseHandler()) {
    LOG(ERROR) << "UseHandler";
    return EXIT_FAILURE;
  }

  // Launch another process that hangs.
  base::FilePath test_executable = Paths::Executable();
  std::wstring child_test_executable =
      test_executable.DirName().Append(L"hanging_program.exe").value();
  ChildLauncher child(child_test_executable, argv[1]);
  child.Start();

  // Wait until it's ready.
  char c;
  if (!LoggingReadFile(child.stdout_read_handle(), &c, sizeof(c)) || c != ' ') {
    LOG(ERROR) << "failed child communication";
    return EXIT_FAILURE;
  }

  if (argc == 3 && wcscmp(argv[2], L"noexception") == 0) {
    client.DumpAndCrashTargetProcess(child.process_handle(), 0, 0);
    return EXIT_SUCCESS;
  } else {
    if (CrashAndDumpTarget(client, child.process_handle()))
      return EXIT_SUCCESS;
  }

  return EXIT_FAILURE;
}

}  // namespace
}  // namespace test
}  // namespace crashpad

int wmain(int argc, wchar_t* argv[]) {
  return crashpad::test::CrashOtherProgram(argc, argv);
}
