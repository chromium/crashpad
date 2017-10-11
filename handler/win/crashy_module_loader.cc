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

#include <stdlib.h>
#include <windows.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "client/crashpad_client.h"

namespace crashpad {
namespace {

int CrashyModuleLoaderMain(int argc, wchar_t* argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %ls <server_pipe_name> <module_path>\n", argv[0]);
    return EXIT_FAILURE;
  }

  CrashpadClient client;
  if (!client.SetHandlerIPCPipe(argv[1])) {
    LOG(ERROR) << "SetHandler";
    return EXIT_FAILURE;
  }

  base::FilePath module_path(argv[2]);
  HMODULE module = LoadLibrary(module_path.value().c_str());
  if (!module) {
    PLOG(ERROR) << "LoadLibrary";
    return EXIT_FAILURE;
  }
  FARPROC crash_me = GetProcAddress(module, "CrashMe");
  if (!crash_me) {
    PLOG(ERROR) << "GetProcAddress";
    return EXIT_FAILURE;
  }
  reinterpret_cast<void(*)()>(crash_me)();

  return EXIT_SUCCESS;
}

}  // namespace
}  // namespace crashpad

int wmain(int argc, wchar_t* argv[]) {
  return crashpad::CrashyModuleLoaderMain(argc, argv);
}
