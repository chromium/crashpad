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

#include <algorithm>

#include "base/logging.h"

namespace crashpad {

class ElfImageReader::ProgramHeaderTable {
 public:
  virtual bool GetDynamicSegment(LinuxVMAddress* address,
                                 LinuxVMSize* size) const = 0;
  virtual bool GetLoadBias(LinuxVMAddress actual,
                           LinuxVMSize* bias) const = 0;
};

template <typename PhdrType>
class ElfImageReader::ProgramHeaderTableSpecific
    : public ElfImageReader::ProgramHeaderTable {
 public:
  ProgramHeaderTableSpecific<PhdrType>() {}
  ~ProgramHeaderTableSpecific<PhdrType>() {}

  bool GetDynamicSegment(LinuxVMAddress* address,
                         LinuxVMSize* size) const override {
    const PhdrType* phdr;
    if (!GetProgramHeader(PT_DYNAMIC, &phdr)) {
      return false;
    }
    *address = phdr->p_vaddr;
    *size = phdr->p_memsz;
    return true;
  }

  bool GetLoadBias(LinuxVMAddress actual, LinuxVMSize* bias) const override {
    for (auto& header : table_) {
      if (header.p_type == PT_LOAD && header.p_offset == 0) {
        *bias = actual - header.p_vaddr;
        return true;
      }
    }
    return false;
  }

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

 private:
  std::vector<PhdrType> table_;

  DISALLOW_COPY_AND_ASSIGN(ProgramHeaderTableSpecific<PhdrType>);
};

class ElfImageReader::ElfDynamicTableReader {
 public:
  ElfDynamicTableReader() {}
  ~ElfDynamicTableReader() {}

  bool Initialize(const ProcessMemory& memory,
                  LinuxVMAddress address,
                  LinuxVMSize size,
                  bool is_64_bit) {
    return is_64_bit ? Read<Elf64_Dyn>(memory, address, size)
                     : Read<Elf32_Dyn>(memory, address, size);
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

  bool GetDebugAddress(LinuxVMAddress* debug) {
    if (have_debug_) {
      *debug = debug_address_;
      return true;
    }
    return false;
  }

 private:
  template <typename DynEnt>
  bool Read(const ProcessMemory& memory,
            LinuxVMAddress address,
            LinuxVMSize size) {
    DynEnt buffer[64];
    while (size > 0) {
      size_t read_size = std::min(size, LinuxVMSize{sizeof(buffer)});
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
          case DT_DEBUG:
            debug_address_ = buffer[index].d_un.d_ptr;
            have_debug_ = true;
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
  LinuxVMAddress debug_address_ = 0;
  bool have_symbol_table_ = false;
  bool have_string_table_ = false;
  bool have_string_table_size_ = false;
  bool have_debug_ = false;

  DISALLOW_COPY_AND_ASSIGN(ElfDynamicTableReader);
};

ElfImageReader::ElfImageReader()
    : header64_(),
      base_address_(0),
      memory_(nullptr),
      program_headers_(),
      dynamic_table_(),
      symbol_table_(),
      dynamic_table_initialized_(),
      symbol_table_initialized_(),
      is_64_bit_(true) {}

ElfImageReader::~ElfImageReader() {}

bool ElfImageReader::Initialize(const ProcessMemory* memory,
                                LinuxVMAddress address) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);
  memory_ = memory;
  base_address_ = address;

  uint8_t e_ident[EI_NIDENT];
  if (!memory_->Read(base_address_, EI_NIDENT, e_ident)) {
    return false;
  }

  if (e_ident[EI_MAG0] != ELFMAG0 || e_ident[EI_MAG1] != ELFMAG1 ||
      e_ident[EI_MAG2] != ELFMAG2 || e_ident[EI_MAG3] != ELFMAG3) {
    LOG(ERROR) << "Incorrect ELF magic number";
    return false;
  }

  is_64_bit_ = e_ident[EI_CLASS] == 2;

  if (!(is_64_bit_
          ? memory_->Read(base_address_, sizeof(header64_), &header64_)
          : memory_->Read(base_address_, sizeof(header32_), &header32_))) {
    return false;
  }

  if (!InitializeProgramHeaders()) {
    return false;
  }

  if (!program_headers_.get()->GetLoadBias(base_address_, &load_bias_)) {
    LOG(ERROR) << "couldn't determine load bias";
    return false;
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

uint16_t ElfImageReader::FileType() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return is_64_bit_ ? header64_.e_type : header32_.e_type;
}

bool ElfImageReader::GetSymbol(const std::string& name,
                               LinuxVMAddress* address,
                               LinuxVMSize* size) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  if (!symbol_table_initialized_.is_valid() && !InitializeSymbolTable()) {
    return false;
  }

  ElfSymbolTableReader::SymbolInformation info;
  if (!symbol_table_->GetSymbol(name, &info)) {
    return false;
  }
  *address = GetLoadBias() + info.address;
  *size = info.size;
  return true;
}

bool ElfImageReader::ReadStringTableAtOffset(LinuxVMSize offset,
                                             std::string* string) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  if (!dynamic_table_initialized_.is_valid() && !InitializeDynamicTable()) {
    return false;
  }

  LinuxVMAddress string_table_address;
  LinuxVMSize string_table_size;
  if (!dynamic_table_->GetStringTableInfo(&string_table_address,
                                          &string_table_size)) {
    LOG(ERROR) << "no string table";
    return false;
  }
  if (offset > string_table_size) {
    LOG(ERROR) << "bad offset";
    return false;
  }
  // The gnu linker updates the dynamic section according to the load bias while
  // the Android linker does not.
#if defined(OS_ANDROID)
  string_table_address += GetLoadBias();
#endif

  return memory_->ReadCStringSizeLimited(
      string_table_address + offset, string_table_size - offset, string);
}

bool ElfImageReader::GetDebugAddress(LinuxVMAddress* debug) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  if (!dynamic_table_initialized_.is_valid() && !InitializeDynamicTable()) {
    return false;
  }
  return dynamic_table_->GetDebugAddress(debug);
}

bool ElfImageReader::InitializeProgramHeaders() {
#define INITIALIZE_PROGRAM_HEADERS(PhdrType, header)                     \
  do {                                                                   \
    auto phdrs = new ProgramHeaderTableSpecific<PhdrType>();             \
    program_headers_.reset(phdrs);                                       \
    if (!phdrs->Read(                                                    \
            *memory_, base_address_ + header.e_phoff, header.e_phnum)) { \
      return false;                                                      \
    }                                                                    \
  } while (0);

  if (is_64_bit_) {
    INITIALIZE_PROGRAM_HEADERS(Elf64_Phdr, header64_);
  } else {
    INITIALIZE_PROGRAM_HEADERS(Elf32_Phdr, header32_);
  }
  return true;
}

bool ElfImageReader::InitializeDynamicTable() {
  dynamic_table_initialized_.set_invalid();

  LinuxVMAddress dyn_table_address;
  LinuxVMSize dyn_table_size;
  if (!program_headers_.get()->GetDynamicSegment(&dyn_table_address,
                                                 &dyn_table_size)) {
    LOG(ERROR) << "no dynamic table";
    return false;
  }
  dyn_table_address += GetLoadBias();

  dynamic_table_.reset(new ElfDynamicTableReader());
  if (!dynamic_table_->Initialize(
          *memory_, dyn_table_address, dyn_table_size, is_64_bit_)) {
    return false;
  }
  dynamic_table_initialized_.set_valid();
  return true;
}

bool ElfImageReader::InitializeSymbolTable() {
  symbol_table_initialized_.set_invalid();
  if (dynamic_table_initialized_.is_uninitialized() &&
      !InitializeDynamicTable()) {
    return false;
  }
  LinuxVMAddress symbol_table_address;
  if (!dynamic_table_->GetSymbolTableAddress(&symbol_table_address)) {
    LOG(ERROR) << "No symbol table";
    return false;
  }

  // The gnu linker updates the dynamic section according to the load bias while
  // the Android linker does not.
#if defined(OS_ANDROID)
  symbol_table_address += GetLoadBias();
#endif

  symbol_table_.reset(new ElfSymbolTableReader(
      memory_, this, symbol_table_address, is_64_bit_));
  symbol_table_initialized_.set_valid();
  return true;
}

}  // namespace crashpad
