// Copyright 2014 The Crashpad Authors. All rights reserved.
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

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include <algorithm>

#if defined(__APPLE__) || defined(__linux__)
#define OS_POSIX 1
#elif defined(_WIN32)
#define OS_WIN 1
#endif

#if defined(OS_POSIX)
#include <unistd.h>
#elif defined(OS_WIN)
#include <windows.h>
#endif

#if defined(OS_WIN)

namespace {

// Various semi-documented NT internals to retrieve open handles.

typedef enum _SYSTEM_INFORMATION_CLASS {
  SystemHandleInformation = 16
} SYSTEM_INFORMATION_CLASS;

typedef struct _SYSTEM_HANDLE_INFORMATION {
  USHORT ProcessId;
  USHORT CreatorBackTraceIndex;
  UCHAR ObjectTypeNumber;
  UCHAR Flags;
  USHORT Handle;
  PVOID Object;
  ACCESS_MASK GrantedAccess;
} SYSTEM_HANDLE_INFORMATION, *PSYSTEM_HANDLE_INFORMATION;

typedef struct _SYSTEM_HANDLE_INFORMATION_EX {
  ULONG NumberOfHandles;
  SYSTEM_HANDLE_INFORMATION Information[1];
} SYSTEM_HANDLE_INFORMATION_EX, *PSYSTEM_HANDLE_INFORMATION_EX;

typedef NTSTATUS(WINAPI* NTQUERYSYSTEMINFORMATION)(
    SYSTEM_INFORMATION_CLASS SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength,
    PULONG ReturnLength);

void EnsureOnlyStdioHandlesOpen() {
  // Initialize the NTAPI functions we need.
  HMODULE ntdll_handle = GetModuleHandle(L"ntdll.dll");
  if (!ntdll_handle) {
    fprintf(stderr, "GetModuleHandle ntdll.dll failed.\n");
    abort();
  }

  NTQUERYSYSTEMINFORMATION NtQuerySystemInformation;
  NtQuerySystemInformation = reinterpret_cast<NTQUERYSYSTEMINFORMATION>(
                      GetProcAddress(ntdll_handle, "NtQuerySystemInformation"));
  if (!NtQuerySystemInformation) {
    fprintf(stderr, "GetProcAddress NtQuerySystemInformation failed.\n");
    abort();
  }

  // Get the number of handles on the system.
  DWORD buffer_size = 0;
  SYSTEM_HANDLE_INFORMATION_EX temp_info;
  NTSTATUS status = NtQuerySystemInformation(
      SystemHandleInformation, &temp_info, sizeof(temp_info), &buffer_size);
  if (!buffer_size) {
    fprintf(stderr,
            "NtQuerySystemInformation for number of handles failed: 0x%lX\n",
            status);
    abort();
  }

  SYSTEM_HANDLE_INFORMATION_EX *system_handles =
      reinterpret_cast<SYSTEM_HANDLE_INFORMATION_EX*>(new BYTE[buffer_size]);

  // This is likely flaky as we're racing with other handles being created on
  // the system between the size query above, and the actual retrieval here.
  status = NtQuerySystemInformation(SystemHandleInformation, system_handles,
                                    buffer_size, &buffer_size);
  if (status != 0) {
    fprintf(stderr, "Failed to get the handle list: 0x%lX\n", status);
    delete[] system_handles;
    abort();
  }

  for (ULONG i = 0; i < system_handles->NumberOfHandles; ++i) {
    USHORT h = system_handles->Information[i].Handle;
    if (system_handles->Information[i].ProcessId != GetCurrentProcessId())
      continue;

    // TODO(scottmg): This is probably insufficient, we'll need to allow having
    // a few other standard handles open (for example, to the window station),
    // or only check for handles of certain types.
    HANDLE handle = reinterpret_cast<HANDLE>(h);
    if (handle != GetStdHandle(STD_INPUT_HANDLE) &&
        handle != GetStdHandle(STD_OUTPUT_HANDLE) &&
        handle != GetStdHandle(STD_ERROR_HANDLE)) {
      fprintf(stderr, "Handle 0x%lX is not stdio handle\n", handle);
      abort();
    }
  }

  delete [] system_handles;
}

}  // namespace

#endif  // OS_WIN

int main(int argc, char* argv[]) {
#if defined(OS_POSIX)
  // Make sure that there’s nothing open at any FD higher than 3. All FDs other
  // than stdin, stdout, and stderr should have been closed prior to or at
  // exec().
  int max_fd = std::max(static_cast<int>(sysconf(_SC_OPEN_MAX)), OPEN_MAX);
  max_fd = std::max(max_fd, getdtablesize());
  for (int fd = STDERR_FILENO + 1; fd < max_fd; ++fd) {
    if (close(fd) == 0 || errno != EBADF) {
      abort();
    }
  }

  // Read a byte from stdin, expecting it to be a specific value.
  char c;
  ssize_t rv = read(STDIN_FILENO, &c, 1);
  if (rv != 1 || c != 'z') {
    abort();
  }

  // Write a byte to stdout.
  c = 'Z';
  rv = write(STDOUT_FILENO, &c, 1);
  if (rv != 1) {
    abort();
  }
#elif defined(OS_WIN)
  // Make sure there's nothing open other than stdin, stdout, and stderr.
  EnsureOnlyStdioHandlesOpen();

  // Read a byte from stdin, expecting it to be a specific value.
  char c;
  DWORD bytes_read;
  if (!ReadFile(GetStdHandle(STD_INPUT_HANDLE), &c, 1, &bytes_read, nullptr) ||
      bytes_read != 1 || c != 'z') {
    abort();
  }

  // Write a byte to stdout.
  c = 'Z';
  DWORD bytes_written;
  if (!WriteFile(
          GetStdHandle(STD_OUTPUT_HANDLE), &c, 1, &bytes_written, nullptr) ||
      bytes_written != 1) {
    abort();
  }
#endif  // OS_POSIX

  return 0;
}
