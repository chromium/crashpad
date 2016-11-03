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

#include "base/debug/alias.h"
#include "base/logging.h"
#include "base/macros.h"
#include "client/crashpad_client.h"
#include "client/crashpad_info.h"

DWORD WINAPI Thread1(LPVOID dummy) {
  // We set the thread priority up by one as a hacky way to signal to the other
  // test program that this is the thread we want to dump.
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
  Sleep(INFINITE);
  return 0;
}

DWORD WINAPI Thread2(LPVOID dummy) {
  Sleep(INFINITE);
  return 0;
}

DWORD WINAPI Thread3(LPVOID dummy) {
  HMODULE dll = LoadLibrary(L"loader_lock_dll.dll");
  if (!dll)
    PLOG(ERROR) << "LoadLibrary";

  // This call is not expected to return.
  if (!FreeLibrary(dll))
    PLOG(ERROR) << "FreeLibrary";

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

  // Make sure this module has a CrashpadInfo structure.
  crashpad::CrashpadInfo* crashpad_info =
      crashpad::CrashpadInfo::GetCrashpadInfo();
  base::debug::Alias(crashpad_info);

  HANDLE threads[3];
  threads[0] = CreateThread(nullptr, 0, Thread1, nullptr, 0, nullptr);
  threads[1] = CreateThread(nullptr, 0, Thread2, nullptr, 0, nullptr);
  threads[2] = CreateThread(nullptr, 0, Thread3, nullptr, 0, nullptr);

  // Our whole process is going to hang when the loaded DLL hangs in its
  // DllMain(), so we can't signal to our parent that we're "ready". So, use a
  // hokey delay of 1s after we spawn the threads, and hope that we make it to
  // the FreeLibrary call by then.
  Sleep(1000);

  fprintf(stdout, " ");
  fflush(stdout);

  WaitForMultipleObjects(ARRAYSIZE(threads), threads, true, INFINITE);

  return 0;
}
