// Copyright 2018 The Crashpad Authors. All rights reserved.
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

#include "util/process/process_memory_fuchsia.h"

#include <zircon/syscalls.h>

#include "base/logging.h"
#include "base/fuchsia/fuchsia_logging.h"

namespace crashpad {

ProcessMemoryFuchsia::ProcessMemoryFuchsia() : ProcessMemory(), process_() {}

ProcessMemoryFuchsia::~ProcessMemoryFuchsia() {}

bool ProcessMemoryFuchsia::Initialize(zx_handle_t process) {
  process_ = process;
  return true;
}

bool ProcessMemoryFuchsia::Read(VMAddress address,
                                size_t size,
                                void* buffer) const {
  size_t actual;
  zx_status_t status =
      zx_process_read_memory(process_, address, buffer, size, &actual);

  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "zx_process_read_memory";
    return false;
  }

  if (actual != size) {
    LOG(ERROR) << "zx_process_read_memory short read";
    return false;
  }

  return true;
}

bool ProcessMemoryFuchsia::ReadCStringInternal(VMAddress address,
                                               bool has_size,
                                               size_t size,
                                               std::string* string) const {
  string->clear();

  char buffer[4096];
  do {
    size_t read_size;
    if (has_size) {
      read_size = std::min(sizeof(buffer), size);
    } else {
      read_size = sizeof(buffer);
    }
    size_t bytes_read;
    zx_status_t status = zx_process_read_memory(
        process_, address, buffer, read_size, &bytes_read);
    if (status != ZX_OK) {
      ZX_LOG(ERROR, status) << "zx_process_read_memory";
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
