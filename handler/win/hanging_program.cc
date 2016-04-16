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

#include <stdio.h>
#include <windows.h>

#include "base/logging.h"
#include "base/macros.h"
#include "client/crashpad_client.h"
#include "client/crashpad_info.h"

#if defined(COMPILER_MSVC)
#pragma optimize("", off)
#endif

void Alias(const void* var) {
}

#if defined(COMPILER_MSVC)
#pragma optimize("", on)
#endif

DWORD WINAPI Thread1(LPVOID dummy) {
  Sleep(INFINITE);
  return 0;
}

DWORD WINAPI Thread2(LPVOID dummy) {
  Sleep(INFINITE);
  return 0;
}

int wmain(int argc, wchar_t* argv[]) {
  crashpad::CrashpadClient client;

  if (argc == 2) {
    if (!client.SetHandlerIPCPipe(argv[1])) {
      LOG(ERROR) << "SetHandler";
      return EXIT_FAILURE;
    }
  } else {
    fprintf(stderr, "Usage: %ls <server_pipe_name>\n", argv[0]);
    return EXIT_FAILURE;
  }

  if (!client.UseHandler()) {
    LOG(ERROR) << "UseHandler";
    return EXIT_FAILURE;
  }

  // Make sure this module has a CrashpadInfo structure.
  crashpad::CrashpadInfo* crashpad_info =
      crashpad::CrashpadInfo::GetCrashpadInfo();
  Alias(crashpad_info);

  HANDLE threads[2];
  threads[0] = CreateThread(nullptr, 0, Thread1, nullptr, 0, nullptr);
  threads[1] = CreateThread(nullptr, 0, Thread2, nullptr, 0, nullptr);

  fprintf(stdout, " ");
  fflush(stdout);

  WaitForMultipleObjects(ARRAYSIZE(threads), threads, true, INFINITE);

  return 0;
}
