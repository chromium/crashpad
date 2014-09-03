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
#include <unistd.h>

#include <algorithm>

int main(int argc, char* argv[]) {
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

  return 0;
}
