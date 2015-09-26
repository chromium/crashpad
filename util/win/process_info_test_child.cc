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

#include <intrin.h>
#include <stdlib.h>
#include <stdint.h>
#include <wchar.h>
#include <windows.h>

// A simple binary to be loaded and inspected by ProcessInfo.
int wmain(int argc, wchar_t** argv) {
  if (argc != 2)
    abort();

  // Get a handle to the event we use to communicate with our parent.
  HANDLE done_event = CreateEvent(nullptr, true, false, argv[1]);
  if (!done_event)
    abort();

  // Load an unusual module (that we don't depend upon) so we can do an
  // existence check.
  if (!LoadLibrary(L"lz32.dll"))
    abort();

  HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
  if (out == INVALID_HANDLE_VALUE)
    abort();
  // We just want any valid address that's known to be code.
  uint64_t code_address = reinterpret_cast<uint64_t>(_ReturnAddress());
  DWORD bytes_written;
  if (!WriteFile(
          out, &code_address, sizeof(code_address), &bytes_written, nullptr) ||
      bytes_written != sizeof(code_address)) {
    abort();
  }

  if (WaitForSingleObject(done_event, INFINITE) != WAIT_OBJECT_0)
    abort();

  CloseHandle(done_event);

  return EXIT_SUCCESS;
}
