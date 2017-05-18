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

#ifndef CRASHPAD_UTIL_LINUX_PROCESS_TYPES_H_
#define CRASHPAD_UTIL_LINUX_PROCESS_TYPES_H_

#include <stdint.h>

namespace crashpad {
namespace process_types {

template <typename Traits>
struct elf_header {
  uint8_t e_ident[16];
  uint16_t e_type;
  uint16_t e_machine;
  uint32_t e_version;
  typename Traits::Pointer e_entry;
  typename Traits::ULong e_phoff;
  typename Traits::ULong e_shoff;
  uint32_t e_flags;
  uint16_t e_ehsize;
  uint16_t e_phentsize;
  uint16_t e_phnum;
  uint16_t e_shentsize;
  uint16_t e_shnum;
  uint16_t e_shstrndx;
};

template <typename Traits>
struct section {
  uint32_t sh_name;
  uint32_t sh_type;
  typename Traits::ULong sh_flags;
  typename Traits::Pointer sh_addr;
  typename Traits::ULong sh_offset;
  typename Traits::ULong sh_size;
  uint32_t sh_link;
  uint32_t sh_info;
  typename Traits::ULong sh_addralign;
  typename Traits::ULong sh_entsize;
};

}  // process_types
}  // namespace crashpad

#endif  // CRASHPAD_UTIL_LINUX_PROCESS_TYPES_H_
