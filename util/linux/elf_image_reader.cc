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

#include "util/linux/elf_image_reader.h"

#include <elf.h>

#include <algorithm>

#include "base/logging.h"
#include "util/misc/implicit_cast.h"

namespace crashpad {

namespace {

using Phdr32 = process_types::program_header<internal::Traits32>;
using Phdr64 = process_types::program_header<internal::Traits64>;

template <typename PhdrType>
class ProgramHeaderTableSpecific : public ElfImageReader::ProgramHeaderTable {
 public:
  bool GetProgramHeader(uint32_t type, const PhdrType** header_out) const {
    for (auto& header : table_) {
      if (header.p_type == type) {
        *header_out = &header;
        return true;
      }
    }
    return false;
  }

  bool Read(const ProcessMemory& memory,
            LinuxVMAddress address,
            LinuxVMSize num_segments) {
    table_.resize(num_segments);
    return memory.Read(address, sizeof(PhdrType) * num_segments, table_.data());
  }

  std::vector<PhdrType> table_;
};

template <typename Traits>
bool ReadElfHeader(const ProcessMemory& memory,
                   LinuxVMAddress address,
                   process_types::elf_header<Traits>* header) {
  return memory.Read(address, sizeof(*header), header);
}

class ElfDynamicTable {
 public:
  bool Initialize(const ProcessMemory& memory,
                  LinuxVMAddress address,
                  LinuxVMSize size,
                  bool is_64_bit) {
    return is_64_bit ? ReadDynamicTable<Elf64_Dyn>(memory, address, size)
                     : ReadDynamicTable<Elf32_Dyn>(memory, address, size);
  }

  bool GetSymbolTableAddress(LinuxVMAddress* address) {
    if (have_symbol_table_) {
      *address = symbol_table_address_;
      return true;
    }
    return false;
  }

  bool GetStringTableInfo(LinuxVMAddress* address, LinuxVMSize* size) {
    if (have_string_table_ && have_string_table_size_) {
      *address = string_table_address_;
      *size = string_table_size_;
      return true;
    }
    return false;
  }

 private:
  template <typename DynEnt>
  bool ReadDynamicTable(const ProcessMemory& memory,
                        LinuxVMAddress address,
                        LinuxVMSize size) {
    DynEnt buffer[64];
    while (size > 0) {
      size_t read_size = std::min(size, LinuxVMSize{sizeof(buffer)});
      LOG(INFO) << "Reading " << read_size << " bytes of dynamic table at "
                << address;
      if (!memory.Read(address, read_size, buffer)) {
        return false;
      }
      size -= read_size;
      address += read_size;
      for (size_t index = 0; index < (read_size / sizeof(DynEnt)); ++index) {
        switch (buffer[index].d_tag) {
          case DT_NULL:
            return true;
          case DT_STRTAB:
            string_table_address_ = buffer[index].d_un.d_ptr;
            have_string_table_ = true;
            break;
          case DT_SYMTAB:
            symbol_table_address_ = buffer[index].d_un.d_ptr;
            have_symbol_table_ = true;
            break;
          case DT_STRSZ:
            string_table_size_ = buffer[index].d_un.d_val;
            have_string_table_size_ = true;
            break;
          default:
            break;
        }
      }
    }
    return false;
  }

  LinuxVMAddress symbol_table_address_ = 0;
  LinuxVMAddress string_table_address_ = 0;
  LinuxVMSize string_table_size_ = 0;
  bool have_symbol_table_ = false;
  bool have_string_table_ = false;
  bool have_string_table_size_ = false;
};

bool ReadStringTable(const ProcessMemory& memory,
                     LinuxVMAddress address,
                     LinuxVMSize size,
                     std::vector<std::string>* string_table) {
  string_table->clear();

  while (size > 0) {
    std::string value;
    if (!memory.ReadCStringSizeLimited(address, size, &value)) {
      return false;
    }
    address += value.size() + 1;
    size -= value.size() + 1;
    string_table->push_back(value);
  }
  return true;
}

}  // namespace

ElfImageReader::ElfImageReader()
    : header64(),
      program_headers_(),
      string_table_(),
      symbol_table_(),
      base_address_(0),
      symbol_table_initialized_(),
      memory_(nullptr),
      is_64_bit_(true) {}

ElfImageReader::~ElfImageReader() {}

bool ElfImageReader::Initialize(const ProcessMemory* memory,
                                LinuxVMAddress address,
                                const std::string& name) {
  memory_ = memory;
  base_address_ = address;

  uint8_t e_ident[EI_NIDENT];
  if (!memory_->Read(base_address_, EI_NIDENT, e_ident)) {
    return false;
  }

  if (e_ident[EI_MAG0] != ELFMAG0 || e_ident[EI_MAG1] != ELFMAG1 ||
      e_ident[EI_MAG2] != ELFMAG2 || e_ident[EI_MAG3] != ELFMAG3) {
    LOG(ERROR) << "Incorrect ELF magic number";
    LOG(INFO) << std::hex << e_ident[EI_MAG0];
    LOG(INFO) << std::hex << e_ident[EI_MAG1];
    LOG(INFO) << std::hex << e_ident[EI_MAG2];
    LOG(INFO) << std::hex << e_ident[EI_MAG3];
    return false;
  }

  is_64_bit_ = e_ident[EI_CLASS] == 2;

#define INITIALIZE_HEADER(traits, header) \
  ReadElfHeader<traits>(*memory_, base_address_, header)
  return is_64_bit_ ? INITIALIZE_HEADER(internal::Traits64, &header64)
                    : INITIALIZE_HEADER(internal::Traits32, &header32);
#undef INITIALIZE_HEADER
}

bool ElfImageReader::GetSymbol(const std::string& name,
                               LinuxVMAddress* address,
                               LinuxVMSize* size) {
  if (symbol_table_initialized_.is_uninitialized() && !InitializeSymbols()) {
    return false;
  }

  return symbol_table_.GetSymbol(name, address, size);
}

bool ElfImageReader::InitializeProgramHeaders() {
  if (is_64_bit_) {
    auto phdrs = new ProgramHeaderTableSpecific<Phdr64>();
    program_headers_.reset(phdrs);
    return phdrs->Read(
        *memory_, base_address_ + header64.e_phoff, header64.e_phnum);
  } else {
    auto phdrs = new ProgramHeaderTableSpecific<Phdr32>();
    program_headers_.reset(phdrs);
    return phdrs->Read(
        *memory_, base_address_ + header32.e_phoff, header32.e_phnum);
  }
}

bool ElfImageReader::InitializeSymbols() {
  if (!InitializeProgramHeaders()) {
    return false;
  }

  ElfDynamicTable dyn_table;
  LinuxVMAddress dyn_table_address;
  LinuxVMSize dyn_table_size;

#define LOCATE_DYNAMIC(PhdrType)                                        \
  do {                                                                  \
    const PhdrType* phdr;                                               \
    auto phdr_table =                                                   \
        static_cast<const ProgramHeaderTableSpecific<PhdrType>* const>( \
            program_headers_.get());                                    \
    if (!phdr_table->GetProgramHeader(PT_DYNAMIC, &phdr)) {             \
      return false;                                                     \
    }                                                                   \
    dyn_table_address = base_address_ + phdr->p_vaddr;                  \
    dyn_table_size = phdr->p_memsz;                                     \
  } while (0);

  if (is_64_bit_) {
    LOCATE_DYNAMIC(Phdr64);
  } else {
    LOCATE_DYNAMIC(Phdr32);
  }
#undef INITIALIZE_DYNAMIC

  if (!dyn_table.Initialize(
          *memory_, dyn_table_address, dyn_table_size, is_64_bit_)) {
    return false;
  }

  LinuxVMAddress string_table_address;
  LinuxVMSize string_table_size;
  if (!dyn_table.GetStringTableInfo(&string_table_address,
                                    &string_table_size)) {
    return false;
  }

  LinuxVMAddress symbol_table_address;
  if (!dyn_table.GetSymbolTableAddress(&symbol_table_address)) {
    return false;
  }

  if (!ReadStringTable(
          *memory_, string_table_address, string_table_size, &string_table_)) {
    return false;
  }

  return symbol_table_.Initialize(
      *memory_, string_table_, symbol_table_address, is_64_bit_);
}

}  // namespace crashpad
