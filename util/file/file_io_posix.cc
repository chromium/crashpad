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

#include "util/file/file_io.h"

#include <unistd.h>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/posix/eintr_wrapper.h"

namespace {

struct ReadTraits {
  using VoidBufferType = void*;
  using CharBufferType = char*;
  static ssize_t Operate(int fd, CharBufferType buffer, size_t size) {
    return read(fd, buffer, size);
  }
};

struct WriteTraits {
  using VoidBufferType = const void*;
  using CharBufferType = const char*;
  static ssize_t Operate(int fd, CharBufferType buffer, size_t size) {
    return write(fd, buffer, size);
  }
};

template <typename Traits>
ssize_t ReadOrWrite(int fd,
                    typename Traits::VoidBufferType buffer,
                    size_t size) {
  typename Traits::CharBufferType buffer_c =
      reinterpret_cast<typename Traits::CharBufferType>(buffer);

  ssize_t total_bytes = 0;
  while (size > 0) {
    ssize_t bytes = HANDLE_EINTR(Traits::Operate(fd, buffer_c, size));
    if (bytes < 0) {
      return bytes;
    } else if (bytes == 0) {
      break;
    }

    buffer_c += bytes;
    size -= bytes;
    total_bytes += bytes;
  }

  return total_bytes;
}

}  // namespace

namespace crashpad {

ssize_t ReadFile(FileHandle file, void* buffer, size_t size) {
  return ReadOrWrite<ReadTraits>(file, buffer, size);
}

ssize_t WriteFile(FileHandle file, const void* buffer, size_t size) {
  return ReadOrWrite<WriteTraits>(file, buffer, size);
}

bool LoggingCloseFile(FileHandle file) {
  int rv = IGNORE_EINTR(close(file));
  PLOG_IF(ERROR, rv != 0) << "close";
  return rv == 0;
}

}  // namespace crashpad
