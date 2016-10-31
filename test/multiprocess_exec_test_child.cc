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
#include <sys/resource.h>
#include <unistd.h>
#elif defined(OS_WIN)
#include <windows.h>
#endif

int main(int argc, char* argv[]) {
#if defined(OS_POSIX)
  // Make sure that there’s nothing open at any FD higher than 3. All FDs other
  // than stdin, stdout, and stderr should have been closed prior to or at
  // exec().
  rlimit rlimit_nofile;
  if (getrlimit(RLIMIT_NOFILE, &rlimit_nofile) != 0) {
    abort();
  }
  for (int fd = STDERR_FILENO + 1;
       fd < static_cast<int>(rlimit_nofile.rlim_cur);
       ++fd) {
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
  // TODO(scottmg): Verify that only the handles we expect to be open, are.

  // Read a byte from stdin, expecting it to be a specific value.
  char c;
  DWORD bytes_read;
  HANDLE stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
  if (!ReadFile(stdin_handle, &c, 1, &bytes_read, nullptr) ||
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
