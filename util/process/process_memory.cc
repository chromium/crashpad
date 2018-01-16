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

#include "util/process/process_memory.h"

#include "base/logging.h"

namespace crashpad {

bool ProcessMemory::ReadCStringInternal(VMAddress address,
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

    if (!Read(address, read_size, buffer)) {
      break;
    }

    char* nul = static_cast<char*>(memchr(buffer, '\0', read_size));
    if (nul != nullptr) {
      string->append(buffer, nul - buffer);
      return true;
    }
    string->append(buffer, read_size);

    address += read_size;
    size -= read_size;
  } while (!has_size || size > 0);

  LOG(ERROR) << "unterminated string";
  return false;
}

}  // namespace crashpad
