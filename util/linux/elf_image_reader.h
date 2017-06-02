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

#ifndef CRASHPAD_UTIL_LINUX_ELF_IMAGE_READER_H_
#define CRASHPAD_UTIL_LINUX_ELF_IMAGE_READER_H_

#include <string>

#include "util/linux/address_types.h"
#include "util/linux/elf_symbol_table_reader.h"
#include "util/linux/process_memory.h"
#include "util/linux/process_types.h"
#include "util/linux/traits.h"
#include "util/misc/initialization_state.h"

namespace crashpad {

class ElfImageReader {
 public:
  ElfImageReader();
  ~ElfImageReader();

  bool Initialize(const ProcessMemory* memory,
                  LinuxVMAddress address,
                  const std::string& name);

  bool Is64Bit() const { return is_64_bit_; }

  bool GetSymbol(const std::string& name,
                 LinuxVMAddress* address,
                 LinuxVMSize* size);

  class ProgramHeaderTable {};

 private:
  bool InitializeProgramHeaders();
  bool InitializeSymbols();

  union {
    process_types::elf_header<internal::Traits32> header32;
    process_types::elf_header<internal::Traits64> header64;
  };
  std::unique_ptr<ProgramHeaderTable> program_headers_;
  std::vector<std::string> string_table_;
  ElfSymbolTableReader symbol_table_;
  LinuxVMAddress base_address_;
  InitializationState symbol_table_initialized_;
  const ProcessMemory* memory_;  // weak
  bool is_64_bit_;
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_LINUX_ELF_IMAGE_READER_H_
