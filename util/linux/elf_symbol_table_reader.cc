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

namespace crashpad {

namespace {}  // namespace

ElfSymbolTableReader::ElfSymbolTableReader() {}

ElfSymbolTableReader::~ElfSymbolTableReader() {}

bool ElfSymbolTableReader::Initialize(
    const ProcessMemory& memory,
    const std::vector<std::string>& string_table,
    LinuxVMAddress address,
    bool is_64_bit) {
  return false;
}

bool ElfSymbolTableReader::GetSymbol(const std::string& name,
                                     LinuxVMAddress* address,
                                     LinuxVMSize* size) {
  return false;
}

}  // namespace crashpad
