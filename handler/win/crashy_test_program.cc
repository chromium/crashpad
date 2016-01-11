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
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <windows.h>
#include <winternl.h>

#include <map>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "client/crashpad_client.h"
#include "util/win/critical_section_with_debug_info.h"
#include "util/win/get_function.h"

// ntstatus.h conflicts with windows.h so define this locally.
#ifndef STATUS_NO_SUCH_FILE
#define STATUS_NO_SUCH_FILE static_cast<NTSTATUS>(0xC000000F)
#endif

namespace crashpad {
namespace {

CRITICAL_SECTION g_test_critical_section;

unsigned char g_test_memory[] = {
  99, 98, 97, 96, 95, 94, 93, 92, 91, 90,
  89, 88, 87, 86, 85, 84, 83, 82, 81, 80,
};

ULONG RtlNtStatusToDosError(NTSTATUS status) {
  static const auto rtl_nt_status_to_dos_error =
      GET_FUNCTION_REQUIRED(L"ntdll.dll", ::RtlNtStatusToDosError);
  return rtl_nt_status_to_dos_error(status);
}

void AllocateMemoryOfVariousProtections() {
  SYSTEM_INFO system_info;
  GetSystemInfo(&system_info);

  const size_t kPageSize = system_info.dwPageSize;

  const uint32_t kPageTypes[] = {
    PAGE_NOACCESS,
    PAGE_READONLY,
    PAGE_READWRITE,
    PAGE_EXECUTE,
    PAGE_EXECUTE_READ,
    PAGE_EXECUTE_READWRITE,

    // PAGE_NOACCESS is invalid with PAGE_GUARD.
    PAGE_READONLY | PAGE_GUARD,
    PAGE_READWRITE | PAGE_GUARD,
    PAGE_EXECUTE | PAGE_GUARD,
    PAGE_EXECUTE_READ | PAGE_GUARD,
    PAGE_EXECUTE_READWRITE | PAGE_GUARD,
  };

  // All of these allocations are leaked, we want to view them in windbg via
  // !vprot.
  void* reserve = VirtualAlloc(
      nullptr, arraysize(kPageTypes) * kPageSize, MEM_RESERVE, PAGE_READWRITE);
  PCHECK(reserve) << "VirtualAlloc MEM_RESERVE";
  uintptr_t reserve_as_int = reinterpret_cast<uintptr_t>(reserve);

  for (size_t i = 0; i < arraysize(kPageTypes); ++i) {
    void* result =
        VirtualAlloc(reinterpret_cast<void*>(reserve_as_int + (kPageSize * i)),
                     kPageSize,
                     MEM_COMMIT,
                     kPageTypes[i]);
    PCHECK(result) << "VirtualAlloc MEM_COMMIT " << i;
  }
}

DWORD WINAPI NullThreadProc(void* param) {
  return 0;
}

// Creates a suspended background thread, and sets EDI/RDI to point at
// g_test_memory so we can confirm it's available in the minidump.
bool CreateThreadWithRegisterPointingToTestMemory() {
  HANDLE thread = CreateThread(
      nullptr, 0, &NullThreadProc, nullptr, CREATE_SUSPENDED, nullptr);
  if (!thread) {
    PLOG(ERROR) << "CreateThread";
    return false;
  }

  CONTEXT context = {0};
  context.ContextFlags = CONTEXT_INTEGER;
  if (!GetThreadContext(thread, &context)) {
    PLOG(ERROR) << "GetThreadContext";
    return false;
  }
#if defined(ARCH_CPU_X86_64)
  context.Rdi = reinterpret_cast<DWORD64>(g_test_memory);
#elif defined(ARCH_CPU_X86)
  context.Edi = reinterpret_cast<DWORD>(g_test_memory);
#endif
  if (!SetThreadContext(thread, &context)) {
    PLOG(ERROR) << "SetThreadContext";
    return false;
  }

  return true;
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

int CrashyMain(int argc, wchar_t* argv[]) {
  CrashpadClient client;

  if (argc == 2) {
    if (!client.SetHandlerIPCPipe(argv[1])) {
      LOG(ERROR) << "SetHandler";
      return EXIT_FAILURE;
    }
  } else if (argc == 3) {
    if (!client.StartHandler(base::FilePath(argv[1]),
                             base::FilePath(argv[2]),
                             std::string(),
                             std::map<std::string, std::string>(),
                             std::vector<std::string>(),
                             false)) {
      LOG(ERROR) << "StartHandler";
      return EXIT_FAILURE;
    }
  } else {
    fprintf(stderr, "Usage: %ls <server_pipe_name>\n", argv[0]);
    fprintf(stderr, "       %ls <handler_path> <database_path>\n", argv[0]);
    return EXIT_FAILURE;
  }

  if (!client.UseHandler()) {
    LOG(ERROR) << "UseHandler";
    return EXIT_FAILURE;
  }

  AllocateMemoryOfVariousProtections();

  if (InitializeCriticalSectionWithDebugInfoIfPossible(
          &g_test_critical_section)) {
    EnterCriticalSection(&g_test_critical_section);
  }

  if (!CreateThreadWithRegisterPointingToTestMemory())
    return EXIT_FAILURE;

  SomeCrashyFunction();

  return EXIT_SUCCESS;
}

}  // namespace
}  // namespace crashpad

int wmain(int argc, wchar_t* argv[]) {
  return crashpad::CrashyMain(argc, argv);
}
