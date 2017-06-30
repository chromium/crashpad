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

#ifndef CRASHPAD_SNAPSHOT_LINUX_ELF_DYNAMIC_ARRAY_READER_H_
#define CRASHPAD_SNAPSHOT_LINUX_ELF_DYNAMIC_ARRAY_READER_H_

#include <stdint.h>

#include <map>

#include "base/logging.h"
#include "base/macros.h"
#include "util/linux/address_types.h"
#include "util/linux/process_memory_range.h"
#include "util/misc/reinterpret_bytes.h"

namespace crashpad {

//! \brief A reader for ELF dynamic arrays mapped into another process.
class ElfDynamicArrayReader {
 public:
  ElfDynamicArrayReader();
  ~ElfDynamicArrayReader();

  //! \brief Initializes the reader.
  //!
  //! This method must be called once on an object and must be successfully
  //! called before any other method in this class may be called.
  //!
  //! \param[in] memory A memory reader for the remote process.
  //! \param[in] address The address in the remote process' address space where
  //!     the ELF dynamic table is loaded.
  //! \param[in] size The maximum number of bytes to read.
  bool Initialize(const ProcessMemoryRange& memory,
                  LinuxVMAddress address,
                  LinuxVMSize size);

  //! \brief Retrieve a value from the array.
  //!
  //! \param[in] tag Specifies which value should be retrieved. The possible
  //!     values for this parameter are the `DT_*` values from `<elf.h>`.
  //! \param[out] value The value, casted to an appropriate type, if found.
  //! \return `true` if the value is found.
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

  DISALLOW_COPY_AND_ASSIGN(ElfDynamicArrayReader);
};

}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_LINUX_ELF_DYNAMIC_ARRAY_READER_H_
