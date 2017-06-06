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

#ifndef CRASHPAD_UTIL_LINUX_ELF_SYMBOL_TABLE_READER_H_
#define CRASHPAD_UTIL_LINUX_ELF_SYMBOL_TABLE_READER_H_

#include <string>

#include "util/linux/address_types.h"
#include "util/linux/process_memory.h"

namespace crashpad {

class ElfImageReader;

class ElfSymbolTableReader {
 public:
  struct SymbolInformation {
    LinuxVMAddress address;
    LinuxVMSize size;
  };

  // TODO(jperaza): Support using .hash and .gnu.hash sections to improve symbol
  // lookup.
  ElfSymbolTableReader(const ProcessMemory* memory,
                       ElfImageReader* reader,
                       LinuxVMAddress address,
                       bool is_64_bit);
  ~ElfSymbolTableReader();

  bool GetSymbol(const std::string& name, SymbolInformation* info);

 private:
  template <typename SymEnt>
  bool ScanSymbolTable(const std::string& name, SymbolInformation* info);

  const ProcessMemory* memory_;  // weak
  ElfImageReader* elf_reader_;  // weak;
  const LinuxVMAddress base_address_;
  const bool is_64_bit_;
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_LINUX_ELF_SYMBOL_TABLE_READER_H_
