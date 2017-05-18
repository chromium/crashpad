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

#include "base/logging.h"

namespace crashpad {

ElfImageReader::ElfImageReader()
    : sections_(),
      section_map_(),
      size_(0) {}

ElfImageReader::~ElfImageReader() {}

bool ElfImageReader::Initialize(const ProcessMemory& memory,
                                LinuxVMAddress address,
                                const std::string& name) {

  LOG(INFO) << "Data at address: 0x" << std::hex << *reinterpret_cast<uint32_t*>(address);
  uint8_t e_ident[EI_NIDENT];
  if (!memory.Read(address, EI_NIDENT, e_ident)) {
    return false;
  }

  if (e_ident[EI_MAG0] != ELFMAG0 ||
      e_ident[EI_MAG1] != ELFMAG1 ||
      e_ident[EI_MAG2] != ELFMAG2 ||
      e_ident[EI_MAG3] != ELFMAG3) {
    LOG(ERROR) << "Incorrect ELF magic number";
    LOG(INFO) << std::hex << e_ident[EI_MAG0];
    LOG(INFO) << std::hex << e_ident[EI_MAG1];
    LOG(INFO) << std::hex << e_ident[EI_MAG2];
    LOG(INFO) << std::hex << e_ident[EI_MAG3];
    return false;
  }
  return true;
}

bool ElfImageReader::GetSectionByName(std::string name,
                                      LinuxVMAddress* address) {

}

}  // namespace crashpad
