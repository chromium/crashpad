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

#include "util/linux/elf_dynamic_table_reader.h"

#include <elf.h>

#include <algorithm>

namespace crashpad {

namespace {

template <typename DynType>
bool Read(const ProcessMemory& memory,
          LinuxVMAddress address,
          LinuxVMSize size,
          std::map<uint64_t, uint64_t>* values) {
  std::map<uint64_t, uint64_t> local_values;

  // TODO(jperaza): We can increase this buffer size after adding a method on
  // ProcessMemory which allows reading fewer bytes than requested. This is
  // because we may not always know the exact, expected size of the dynamic
  // table.
  DynType buffer[1];
  while (size > 0) {
    size_t read_size = std::min(size, LinuxVMSize{sizeof(buffer)});
    if (!memory.Read(address, read_size, buffer)) {
      return false;
    }
    size -= read_size;
    address += read_size;
    for (size_t index = 0; index < (read_size / sizeof(DynType)); ++index) {
      if (buffer[index].d_tag == DT_NULL) {
        values->swap(local_values);
        return true;
      }
      local_values[buffer[index].d_tag] = buffer[index].d_un.d_val;
    }
  }
  return false;
}

}  // namespace

ElfDynamicTableReader::ElfDynamicTableReader() : values_() {}

ElfDynamicTableReader::~ElfDynamicTableReader() {}

bool ElfDynamicTableReader::Initialize(const ProcessMemory& memory,
                                       LinuxVMAddress address,
                                       LinuxVMSize size,
                                       bool is_64_bit) {
  return is_64_bit ? Read<Elf64_Dyn>(memory, address, size, &values_)
                   : Read<Elf32_Dyn>(memory, address, size, &values_);
}

}  // namespace crashpad
