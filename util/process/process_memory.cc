// Copyright 2017 The Crashpad Authors. All rights reserved.
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

#include "util/process/process_memory.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"

namespace crashpad {

ProcessMemory::ProcessMemory() : mem_fd_(), pid_(-1) {}

ProcessMemory::~ProcessMemory() {}

bool ProcessMemory::Initialize(pid_t pid) {
  pid_ = pid;
  char path[32];
  snprintf(path, sizeof(path), "/proc/%d/mem", pid_);
  mem_fd_.reset(HANDLE_EINTR(open(path, O_RDONLY | O_NOCTTY | O_CLOEXEC)));
  if (!mem_fd_.is_valid()) {
    PLOG(ERROR) << "open";
    return false;
  }
  return true;
}

bool ProcessMemory::Read(VMAddress address,
                         size_t size,
                         void* buffer) const {
  DCHECK(mem_fd_.is_valid());

  char* buffer_c = static_cast<char*>(buffer);
  while (size > 0) {
    ssize_t bytes_read =
        HANDLE_EINTR(pread64(mem_fd_.get(), buffer_c, size, address));
    if (bytes_read < 0) {
      PLOG(ERROR) << "pread64";
      return false;
    }
    if (bytes_read == 0) {
      LOG(ERROR) << "unexpected eof";
      return false;
    }
    DCHECK_LE(static_cast<size_t>(bytes_read), size);
    size -= bytes_read;
    address += bytes_read;
    buffer_c += bytes_read;
  }
  return true;
}

bool ProcessMemory::ReadCString(VMAddress address,
                                std::string* string) const {
  return ReadCStringInternal(address, false, 0, string);
}

bool ProcessMemory::ReadCStringSizeLimited(VMAddress address,
                                           size_t size,
                                           std::string* string) const {
  return ReadCStringInternal(address, true, size, string);
}

bool ProcessMemory::ReadCStringInternal(VMAddress address,
                                        bool has_size,
                                        size_t size,
                                        std::string* string) const {
  DCHECK(mem_fd_.is_valid());

  string->clear();

  char buffer[4096];
  do {
    size_t read_size;
    if (has_size) {
      read_size = std::min(sizeof(buffer), size);
    } else {
      read_size = sizeof(buffer);
    }
    ssize_t bytes_read;
    bytes_read =
        HANDLE_EINTR(pread64(mem_fd_.get(), buffer, read_size, address));
    if (bytes_read < 0) {
      PLOG(ERROR) << "pread64";
      return false;
    }
    if (bytes_read == 0) {
      break;
    }
    DCHECK_LE(static_cast<size_t>(bytes_read), read_size);

    char* nul = static_cast<char*>(memchr(buffer, '\0', bytes_read));
    if (nul != nullptr) {
      string->append(buffer, nul - buffer);
      return true;
    }
    string->append(buffer, bytes_read);

    address += bytes_read;
    size -= bytes_read;
  } while (!has_size || size > 0);

  LOG(ERROR) << "unterminated string";
  return false;
}

}  // namespace crashpad
