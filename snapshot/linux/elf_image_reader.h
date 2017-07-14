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

#ifndef CRASHPAD_SNAPSHOT_LINUX_ELF_IMAGE_READER_H_
#define CRASHPAD_SNAPSHOT_LINUX_ELF_IMAGE_READER_H_

#include <elf.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "util/linux/address_types.h"
#include "snapshot/linux/elf_dynamic_array_reader.h"
#include "snapshot/linux/elf_symbol_table_reader.h"
#include "util/linux/process_memory_range.h"
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

  //! \brief Initializes the reader.
  //!
  //! This method must be called once on an object and must be successfully
  //! called before any other method in this class may be called.
  //!
  //! \param[in] memory A memory reader for the remote process.
  //! \param[in] address The address in the remote process' address space where
  //!     the ELF image is loaded.
  bool Initialize(const ProcessMemoryRange& memory, LinuxVMAddress address);

  //! \brief Returns the base address of the image's memory range.
  //!
  //! This may differ from the address passed to Initialize() if the ELF header
  //! is not loaded at the start of the first `PT_LOAD` segment.
  LinuxVMAddress Address() const { return memory_.Base(); }

  //! \brief Returns the size of the range containing all loaded segments for
  //!     this image.
  //!
  //! The size may include memory that is unmapped or mapped to other objects if
  //! this image's `PT_LOAD` segments are not contiguous.
  LinuxVMSize Size() const { return memory_.Size(); }

  //! \brief Returns the file type for the image.
  //!
  //! Possible values include `ET_EXEC` or `ET_DYN` from `<elf.h>`.
  uint16_t FileType() const;

  //! \brief Returns the load bias for the image.
  //!
  //! The load bias is the actual load address minus the preferred load address.
  LinuxVMOffset GetLoadBias() const { return load_bias_; }

  //! \brief Reads information from the dynamic symbol table about the symbol
  //!     identified by \a name.
  //!
  //! \param[in] name The name of the symbol to search for.
  //! \param[out] address The address of the symbol in the target process'
  //!     address space, if found.
  //! \param[out] size The size of the symbol, if found.
  //! \return `true` if the symbol was found.
  bool GetDynamicSymbol(const std::string& name,
                        LinuxVMAddress* address,
                        LinuxVMSize* size);

  //! \brief Reads a `NUL`-terminated C string from this image's dynamic string
  //!     table.
  //!
  //! \param[in] offset the byte offset in the string table to start reading.
  //! \param[out] string the string read.
  //! \return `true` on success. Otherwise `false` with a message logged.
  bool ReadDynamicStringTableAtOffset(LinuxVMSize offset, std::string* string);

  //! \brief Determine the debug address.
  //!
  //! The debug address is a pointer to an `r_debug` struct defined in
  //! `<link.h>`.
  //!
  //! \param[out] debug the debug address, if found.
  //! \return `true` if the debug address was found.
  bool GetDebugAddress(LinuxVMAddress* debug);

 private:
  class ProgramHeaderTable;
  template <typename PhdrType>
  class ProgramHeaderTableSpecific;

  bool InitializeProgramHeaders();
  bool InitializeDynamicArray();
  bool InitializeDynamicSymbolTable();
  bool GetAddressFromDynamicArray(uint64_t tag, LinuxVMAddress* address);

  union {
    Elf32_Ehdr header_32_;
    Elf64_Ehdr header_64_;
  };
  LinuxVMAddress ehdr_address_;
  LinuxVMOffset load_bias_;
  ProcessMemoryRange memory_;
  std::unique_ptr<ProgramHeaderTable> program_headers_;
  std::unique_ptr<ElfDynamicArrayReader> dynamic_array_;
  std::unique_ptr<ElfSymbolTableReader> symbol_table_;
  InitializationStateDcheck initialized_;
  InitializationState dynamic_array_initialized_;
  InitializationState symbol_table_initialized_;

  DISALLOW_COPY_AND_ASSIGN(ElfImageReader);
};

}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_LINUX_ELF_IMAGE_READER_H_
