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

#ifndef CRASHPAD_SNAPSHOT_SANITIZED_SANITIZATION_INFORMATION_H_
#define CRASHPAD_SNAPSHOT_SANITIZED_SANITIZATION_INFORMATION_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "util/misc/address_types.h"
#include "util/process/process_memory_range.h"

namespace crashpad {

//! \brief Struture containing information about how snapshots should be
//!     sanitized.
//!
//! \see ProcessSnapshotSanitized
struct SanitizationInformation {
  //! \brief The address in the client process' address space of a nullptr
  //!     terminated array of NUL-terminated strings. The string values are the
  //!     names of whitelisted annotations. This value is 0 if there is no
  //!     whitelist and all annotations are allowed.
  VMAddress annotations_whitelist_address;

  //! \brief An address in the client process' address space within a module to
  //!     target. When a target module is used, crash dumps are discarded unless
  //!     their program counter or pointers on the crashing thread's stack point
  //!     into the target module. This value is 0 if there is no target module.
  VMAddress target_module_address;

  //! \brief Non-zero if stacks should be sanitized for possible PII.
  uint8_t sanitize_stacks;
};

bool ReadSanitizationInfo(ProcessMemoryRange* range,
                          VMAddress sanitization_info_address,
                          std::vector<std::string>* whitelist,
                          VMAddress* target_module_address,
                          bool* sanitize_stacks);

}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_SANITIZED_SANITIZATION_INFORMATION_H_
