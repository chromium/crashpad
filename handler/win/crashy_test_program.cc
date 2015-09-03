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

#include "base/logging.h"
#include "tools/tool_support.h"

namespace crashpad {
namespace {

void SomeCrashyFunction() {
  volatile int* foo = reinterpret_cast<volatile int*>(7);
  *foo = 42;
}

int CrashyMain(int argc, char* argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <server_pipe_name>\n", argv[0]);
    return 1;
  }

  CrashpadClient client;
  if (!client.SetHandler(argv[1])) {
    LOG(ERROR) << "SetHandler";
    return 1;
  }
  if (!client.UseHandler()) {
    LOG(ERROR) << "UseHandler";
    return 1;
  }

  SomeCrashyFunction();

  return 0;
}

}  // namespace
}  // namespace crashpad

int wmain(int argc, wchar_t* argv[]) {
  return crashpad::ToolSupport::Wmain(argc, argv, crashpad::CrashyMain);
}
