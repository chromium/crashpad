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
#include <wchar.h>
#include <windows.h>

// A simple binary to be loaded and inspected by ProcessInfo.
int wmain(int argc, wchar_t** argv) {
  if (argc != 3)
    abort();

  // Get handles to the events we use to communicate with our parent.
  HANDLE started_event = CreateEvent(nullptr, true, false, argv[1]);
  HANDLE done_event = CreateEvent(nullptr, true, false, argv[2]);
  if (!started_event || !done_event)
    abort();

  // Load an unusual module (that we don't depend upon) so we can do an
  // existence check.
  if (!LoadLibrary(L"lz32.dll"))
    abort();

  if (!SetEvent(started_event))
    abort();

  if (WaitForSingleObject(done_event, INFINITE) != WAIT_OBJECT_0)
    abort();

  CloseHandle(done_event);
  CloseHandle(started_event);

  return EXIT_SUCCESS;
}
