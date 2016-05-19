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

#include "base/logging.h"
#include "client/crashpad_client.h"

namespace crashpad {
namespace {

bool g_ok_to_crash;

void LastFunction() {
  // Trigger a crash as late as possible.
  if (g_ok_to_crash)
    __debugbreak();
}

void CInitializer() {
  atexit(&LastFunction);
}

// Add a C initializer that will allow us to run code as early as possible, so
// that we can be the last thing in the atexit() list at shutdown time,
// allowing us to crash as late as possible.

#define SEGMENT_C_INIT ".CRT$XIM"
#pragma data_seg(SEGMENT_C_INIT)
#pragma data_seg()
typedef void (*InitFunc)(void);
__declspec(allocate(SEGMENT_C_INIT)) InitFunc c_init_funcs[] = {&CInitializer};

int LateCrashyMain(int argc, wchar_t* argv[]) {
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

  if (!client.UseHandler()) {
    LOG(ERROR) << "UseHandler";
    return EXIT_FAILURE;
  }

  g_ok_to_crash = true;

  // Exit normally, the registered atexit() will crash.
  return EXIT_SUCCESS;
}

}  // namespace
}  // namespace crashpad

int wmain(int argc, wchar_t* argv[]) {
  return crashpad::LateCrashyMain(argc, argv);
}

