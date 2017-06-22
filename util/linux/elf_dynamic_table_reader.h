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

#ifndef CRASHPAD_UTIL_LINUX_ELF_DYNAMIC_TABLE_READER_H_
#define CRASHPAD_UTIL_LINUX_ELF_DYNAMIC_TABLE_READER_H_

#include <stdint.h>

#include <map>

#include "base/logging.h"
#include "util/linux/address_types.h"
#include "util/linux/process_memory.h"
#include "util/misc/reinterpret_bytes.h"

namespace crashpad {

//! \brief A reader for ELF dynamic tables mapped into another process.
class ElfDynamicTableReader {
 public:
  ElfDynamicTableReader();
  ~ElfDynamicTableReader();

  //! \brief Initializes the reader.
  //!
  //! This method must be called once on an object and must be successfully
  //! called before any other method in this class may be called.
  //!
  //! \param[in] memory A memory reader for the remote process.
  //! \param[in] address The address in the remote process' address space where
  //!     the ELF dynamic table is loaded.
  //! \param[in] size The maximum number of bytes to read.
  //! \param[in] is_64_bit Whether the dynamic table uses a 64-bit format.
  bool Initialize(const ProcessMemory& memory,
                  LinuxVMAddress address,
                  LinuxVMSize size,
                  bool is_64_bit);

  template <typename V>
  bool GetValue(uint64_t tag, V* value) {
    auto iter = values_.find(tag);
    if (iter == values_.end()) {
      LOG(ERROR) << "tag not found";
      return false;
    }
    return ReinterpretBytes(iter->second, value);
  }

 private:
  std::map<uint64_t, uint64_t> values_;

  DISALLOW_COPY_AND_ASSIGN(ElfDynamicTableReader);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_LINUX_ELF_DYNAMIC_TABLE_READER_H_
