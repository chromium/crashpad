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

#include "util/linux/elf_symbol_table_reader.h"

#include <elf.h>

#include "base/logging.h"
#include "util/linux/elf_image_reader.h"

namespace crashpad {

namespace {}  // namespace

ElfSymbolTableReader::ElfSymbolTableReader(const ProcessMemory* memory,
                                           ElfImageReader* elf_reader,
                                           LinuxVMAddress address,
                                           bool is_64_bit)
    : memory_(memory),
      elf_reader_(elf_reader),
      base_address_(address),
      is_64_bit_(is_64_bit) {}

ElfSymbolTableReader::~ElfSymbolTableReader() {}

bool ElfSymbolTableReader::GetSymbol(const std::string& name,
                                     struct SymbolInformation* info) {
  return is_64_bit_ ? ScanSymbolTable<Elf64_Sym>(name, info)
                    : ScanSymbolTable<Elf32_Sym>(name, info);
}

template <typename SymEnt>
bool ElfSymbolTableReader::ScanSymbolTable(const std::string& name,
                                           struct SymbolInformation* info_out) {
  LinuxVMAddress address = base_address_;
  SymEnt entry;
  while (memory_->Read(address, sizeof(entry), &entry)) {
    std::string string;
    if (!elf_reader_->ReadStringTableAtOffset(entry.st_name, &string)) {
      continue;
    }
    if (string == name) {
      info_out->address = entry.st_value;
      info_out->size = entry.st_size;
      return true;
    }
    address += sizeof(entry);
  }
  return false;
}

}  // namespace crashpad
