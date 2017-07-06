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

#include "snapshot/linux/elf_symbol_table_reader.h"

#include <elf.h>

#include "base/logging.h"
#include "snapshot/linux/elf_image_reader.h"

namespace crashpad {

namespace {

uint8_t GetBinding(const Elf32_Sym& sym) {
  return ELF32_ST_BIND(sym.st_info);
}

uint8_t GetBinding(const Elf64_Sym& sym) {
  return ELF64_ST_BIND(sym.st_info);
}

uint8_t GetType(const Elf32_Sym& sym) {
  return ELF32_ST_TYPE(sym.st_info);
}

uint8_t GetType(const Elf64_Sym& sym) {
  return ELF64_ST_TYPE(sym.st_info);
}

uint8_t GetVisibility(const Elf32_Sym& sym) {
  return ELF32_ST_VISIBILITY(sym.st_other);
}

uint8_t GetVisibility(const Elf64_Sym& sym) {
  return ELF64_ST_VISIBILITY(sym.st_other);
}

}  // namespace

ElfSymbolTableReader::ElfSymbolTableReader(const ProcessMemoryRange* memory,
                                           ElfImageReader* elf_reader,
                                           LinuxVMAddress address)
    : memory_(memory), elf_reader_(elf_reader), base_address_(address) {}

ElfSymbolTableReader::~ElfSymbolTableReader() {}

bool ElfSymbolTableReader::GetSymbol(const std::string& name,
                                     SymbolInformation* info) {
  return memory_->Is64Bit() ? ScanSymbolTable<Elf64_Sym>(name, info)
                            : ScanSymbolTable<Elf32_Sym>(name, info);
}

template <typename SymEnt>
bool ElfSymbolTableReader::ScanSymbolTable(const std::string& name,
                                           SymbolInformation* info_out) {
  LinuxVMAddress address = base_address_;
  SymEnt entry;
  std::string string;
  while (memory_->Read(address, sizeof(entry), &entry) &&
         elf_reader_->ReadDynamicStringTableAtOffset(entry.st_name, &string)) {
    if (string == name) {
      info_out->address = entry.st_value;
      info_out->size = entry.st_size;
      info_out->shndx = entry.st_shndx;
      info_out->binding = GetBinding(entry);
      info_out->type = GetType(entry);
      info_out->visibility = GetVisibility(entry);
      return true;
    }
    address += sizeof(entry);
  }
  return false;
}

}  // namespace crashpad
