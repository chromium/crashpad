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

#include <elf.h>

#include <string>

#include "util/linux/address_types.h"
#include "util/linux/elf_symbol_table_reader.h"
#include "util/linux/process_memory.h"
#include "util/misc/initialization_state.h"
#include "util/misc/initialization_state_dcheck.h"

namespace crashpad {

//! \brief A reader for ELF images mapped into another process.
//!
//! This class is capable of reading both 32-bit and 64-bit images.
class ElfImageReader {
 public:
  ElfImageReader();
  ~ElfImageReader();

  //! \brief Inializes the reader.
  //!
  //! This method must be called once on an object and must be successfully
  //! called before any other method in this class may be called.
  //!
  //! \param[in] memory A memory reader for the remote process.
  //! \param[in] address The address in the remote process' address space where
  //!     the ELF image is loaded.
  bool Initialize(const ProcessMemory* memory, LinuxVMAddress address);

  //! \return `true` if the image is 64-bit.
  bool Is64Bit() const { return is_64_bit_; }

  //! \return the file type for the image.
  uint16_t FileType() const;

  //! \brief Reads symbol table information about the symbol identified by \a
  //!     name.
  //!
  //! \param[in] name The name of the symbol to search for.
  //! \param[out] address The address of the symbol if found.
  //! \param[out] size The size of the symbol if found.
  //! \return `true` if the symbol was found.
  bool GetSymbol(const std::string& name,
                 LinuxVMAddress* address,
                 LinuxVMSize* size);

  //! \brief Reads a string from this image's string table.
  //!
  //! \param[in] offset the byte offset in the string table to start reading.
  //! \param[out] string the string read.
  //! \return `true` on success. Otherwise `false` with a message logged.
  bool ReadStringTableAtOffset(LinuxVMSize offset, std::string* string);

  bool GetDebugAddress(LinuxVMAddress* debug);

 private:
  class ProgramHeaderTable;
  template <typename PhdrType>
  class ProgramHeaderTableSpecific;
  class ElfDynamicTableReader;

  bool InitializeProgramHeaders();
  bool InitializeDynamicTable();
  bool InitializeSymbolTable();

  union {
    Elf32_Ehdr header32_;
    Elf64_Ehdr header64_;
  };
  LinuxVMAddress base_address_;
  const ProcessMemory* memory_;  // weak
  std::unique_ptr<ProgramHeaderTable> program_headers_;
  std::unique_ptr<ElfDynamicTableReader> dynamic_table_;
  std::unique_ptr<ElfSymbolTableReader> symbol_table_;
  InitializationStateDcheck initialized_;
  InitializationState program_headers_initialized_;
  InitializationState dynamic_table_initialized_;
  InitializationState symbol_table_initialized_;
  bool is_64_bit_;
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_LINUX_ELF_IMAGE_READER_H_
