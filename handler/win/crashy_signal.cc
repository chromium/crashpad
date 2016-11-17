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

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "client/crashpad_client.h"

namespace crashpad {
namespace {

int MapSignalType(wchar_t* name) {
  if (wcscmp(name, L"SIGFPE") == 0)
    return SIGFPE;
  if (wcscmp(name, L"SIGABRT") == 0)
    return SIGABRT;
  return 0;
}

int CrashySignalMain(int argc, wchar_t* argv[]) {
  CrashpadClient client;

  int sig;
  if (argc == 3 && (sig = MapSignalType(argv[2])) != 0) {
    if (!client.SetHandlerIPCPipe(argv[1])) {
      fprintf(stderr, "SetHandler\n");
      return EXIT_FAILURE;
    }
  } else {
    fprintf(stderr, "Usage: %ls <server_pipe_name> SIGFPE|SIGABRT\n", argv[0]);
    return EXIT_FAILURE;
  }

  if (sig == SIGFPE) {
    // Unmask divide by zero exceptions, because they're masked by default.
    unsigned int control_word;
    _controlfp_s(
        &control_word, static_cast<unsigned int>(~_EM_ZERODIVIDE), _MCW_EM);
    volatile double x = 14.0;
    volatile double y = 0.0;
    volatile double result = x / y;
    printf("%f\n", result);
  } else if (sig == SIGABRT) {
    abort();
  }

  return EXIT_SUCCESS;
}

}  // namespace
}  // namespace crashpad

int wmain(int argc, wchar_t* argv[]) {
  return crashpad::CrashySignalMain(argc, argv);
}
