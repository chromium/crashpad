// Copyright 2021 The Crashpad Authors. All rights reserved.
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

#include "util/ios/exception_logging.h"

#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/posix/eintr_wrapper.h"

namespace crashpad {
namespace internal {
namespace logging {

void RawLog(const char* message) {
  size_t bytes_written = 0;
  const size_t message_len = strlen(message);
  int rv;
  while (bytes_written < message_len) {
    rv = HANDLE_EINTR(write(
        STDERR_FILENO, message + bytes_written, message_len - bytes_written));
    if (rv < 0) {
      // Give up, nothing we can do now.
      break;
    }
    bytes_written += rv;
  }

  if (message_len > 0 && message[message_len - 1] != '\n') {
    do {
      rv = HANDLE_EINTR(write(STDERR_FILENO, "\n", 1));
      if (rv < 0) {
        // Give up, nothing we can do now.
        break;
      }
    } while (rv != 1);
  }
}

}  // namespace logging
}  // namespace internal
}  // namespace crashpad
