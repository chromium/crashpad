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

#ifndef CRASHPAD_UTIL_LINUX_ELF_IMAGE_SYMBOL_READER_H_
#define CRASHPAD_UTIL_LINUX_ELF_IMAGE_SYMBOL_READER_H_

#include <map>
#include <string>

#include "util/linux/address_types.h"
#include "util/linux/process_memory.h"
#include "util/linux/process_types.h"
#include "util/linux/traits.h"

namespace crashpad {

class ElfSymbolTableReader {
 public:
  struct SymbolInformation {
    LinuxVMAddress address;
    LinuxVMSize size;
  };

  ElfSymbolTableReader();
  ~ElfSymbolTableReader();

  bool Initialize(const ProcessMemory& memory,
                  const std::vector<std::string>& string_table,
                  LinuxVMAddress address,
                  bool is_64_bit);

  bool GetSymbol(const std::string& name,
                 LinuxVMAddress* address,
                 LinuxVMSize* size);

 private:
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_LINUX_ELF_IMAGE_READER_H_
