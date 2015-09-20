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

#include <windows.h>

#include "base/logging.h"
#include "util/file/file_io.h"
#include "util/win/scoped_handle.h"

int wmain(int argc, wchar_t* argv[]) {
  CHECK_EQ(argc, 2);

  crashpad::ScopedKernelHANDLE done(CreateEvent(nullptr, true, false, argv[1]));

  PCHECK(LoadLibrary(L"crashpad_snapshot_test_image_reader_module.dll"))
      << "LoadLibrary";

  HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
  PCHECK(out != INVALID_HANDLE_VALUE) << "GetStdHandle";
  char c = ' ';
  crashpad::CheckedWriteFile(out, &c, sizeof(c));

  CHECK_EQ(WAIT_OBJECT_0, WaitForSingleObject(done.get(), INFINITE));

  return 0;
}

