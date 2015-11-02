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

#include "base/files/file_path.h"
#include "base/logging.h"
#include "client/crashpad_client.h"
#include "util/file/file_io.h"
#include "util/win/address_types.h"

namespace {

__declspec(noinline) crashpad::WinVMAddress CurrentAddress() {
  return reinterpret_cast<crashpad::WinVMAddress>(_ReturnAddress());
}

}  // namespace

int wmain(int argc, wchar_t* argv[]) {
  CHECK_EQ(argc, 2);

  crashpad::CrashpadClient client;
  CHECK(client.SetHandlerIPCPipe(argv[1]));
  CHECK(client.UseHandler());

  HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
  PCHECK(out != INVALID_HANDLE_VALUE) << "GetStdHandle";
  crashpad::WinVMAddress break_address = CurrentAddress();
  crashpad::CheckedWriteFile(out, &break_address, sizeof(break_address));

  __debugbreak();

  return 0;
}
