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

#pragma pack(push, 1)

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
struct program_header {
  uint32_t p_type;
  typename Traits::Reserved32_64Only p_flags64;
  typename Traits::ULong p_offset;
  typename Traits::Pointer p_vaddr;
  typename Traits::Pointer p_paddr;
  typename Traits::ULong p_filesz;
  typename Traits::ULong p_memsz;
  typename Traits::Reserved32_32Only p_flags32;
  typename Traits::ULong p_align;
};

#pragma pack(pop)

}  // process_types
}  // namespace crashpad

#endif  // CRASHPAD_UTIL_LINUX_PROCESS_TYPES_H_
