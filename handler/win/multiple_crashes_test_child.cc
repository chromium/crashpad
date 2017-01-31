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

#include <windows.h>

#include "base/logging.h"
#include "client/crashpad_client.h"
#include "client/crashpad_client.h"
#include "util/file/file_io.h"

namespace crashpad {

// Receives an event on the command line, blocks until it's signalled, and then
// crashes immediately. This allows for a stress test on the handler of
// receiving many simultaneous crashes.
int MultipleCrashesMain(int argc, wchar_t* argv[]) {
  CHECK_EQ(argc, 3);

  crashpad::CrashpadClient client;
  CHECK(client.SetHandlerIPCPipe(argv[1]));

  HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
  PCHECK(out != INVALID_HANDLE_VALUE) << "GetStdHandle";

  ScopedKernelHANDLE crash_event(OpenEvent(EVENT_ALL_ACCESS, false, argv[2]));
  CHECK(crash_event.is_valid());

  char c = ' ';
  crashpad::CheckedWriteFile(out, &c, sizeof(c));

  CHECK(WaitForSingleObject(crash_event.get(), INFINITE) == WAIT_OBJECT_0);

  __debugbreak();

  return 0;
}

}  // namespace crashpad

int wmain(int argc, wchar_t* argv[]) {
  return crashpad::MultipleCrashesMain(argc, argv);
}
