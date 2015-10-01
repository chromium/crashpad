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
#include <winternl.h>

// ntstatus.h conflicts with windows.h so define this locally.
#ifndef STATUS_NO_SUCH_FILE
#define STATUS_NO_SUCH_FILE static_cast<NTSTATUS>(0xC000000F)
#endif

#include "base/logging.h"
#include "client/crashpad_client.h"
#include "tools/tool_support.h"

namespace crashpad {
namespace {

ULONG RtlNtStatusToDosError(NTSTATUS status) {
  static decltype(::RtlNtStatusToDosError)* rtl_nt_status_to_dos_error =
      reinterpret_cast<decltype(::RtlNtStatusToDosError)*>(
          GetProcAddress(LoadLibrary(L"ntdll.dll"), "RtlNtStatusToDosError"));
  DCHECK(rtl_nt_status_to_dos_error);
  return rtl_nt_status_to_dos_error(status);
}

void SomeCrashyFunction() {
  // SetLastError and NTSTATUS so that we have something to view in !gle in
  // windbg. RtlNtStatusToDosError() stores STATUS_NO_SUCH_FILE into the
  // LastStatusError of the TEB as a side-effect, and we'll be setting
  // ERROR_FILE_NOT_FOUND for GetLastError().
  SetLastError(RtlNtStatusToDosError(STATUS_NO_SUCH_FILE));
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
