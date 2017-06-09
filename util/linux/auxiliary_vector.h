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

#ifndef CRASHPAD_UTIL_LINUX_AUXILIARY_VECTOR_H_
#define CRASHPAD_UTIL_LINUX_AUXILIARY_VECTOR_H_

#include <string.h>
#include <sys/types.h>

#include <algorithm>
#include <map>

#include "base/logging.h"
#include "base/macros.h"

namespace crashpad {

//! \brief Read the auxiliary vector for a target process.
class AuxiliaryVector {
 public:
  AuxiliaryVector();
  ~AuxiliaryVector();

  //! \brief Initializes this object with the auxiliary vector for the process
  //!     with process ID \a pid.
  //!
  //! This method must be called successfully prior to calling any other method
  //! in this class.
  //!
  //! \param[in] pid The process ID of a target process.
  //! \param[in] is_64_bit Whether the target process is 64-bit.
  //!
  //! \return `true` on success, `false` on failure with a message logged.
  bool Initialize(pid_t pid, bool is_64_bit);

  //! \brief Retrieve a value from the vector.
  //!
  //! \param[in] type Specifies which value should be retrieved. The possible
  //!     values for this parameter are defined by `<linux/auxvec.h>`.
  //! \param[out] value The value, casted to an appropriate type, if found.
  //! \return `true` if the value is found.
  template <typename V>
  bool GetValue(uint64_t type, V* value) const {
    auto iter = values_.find(type);
    if (iter == values_.end()) {
      LOG(ERROR) << "value not found";
      return false;
    }

    if (sizeof(V) < sizeof(uint64_t)) {
      const uint64_t zero = 0;
      auto extra_bytes = reinterpret_cast<const char*>(&(iter->second));
#if defined(ARCH_CPU_LITTLE_ENDIAN)
      extra_bytes += sizeof(V);
#endif  // ARCH_CPU_LITTLE_ENDIAN
      if (memcmp(extra_bytes,
                 &zero,
                 sizeof(uint64_t) - sizeof(V)) != 0) {
        LOG(ERROR) << "information loss";
        return false;
      }
    }

    memset(value, 0, sizeof(V));
#if defined(ARCH_CPU_LITTLE_ENDIAN)
    memcpy(value, &(iter->second), std::min(sizeof(V), sizeof(uint64_t)));
#else
    auto dest_start = reinterpret_cast<char*>(value);
    auto source_start = reinterpret_cast<const char*>(&(iter->second));
    memcpy(std::max(dest_start, dest_start + sizeof(V) - sizeof(uint64_t)),
           std::max(source_start, source_start + sizeof(uint64_t) - sizeof(V)),
           std::min(sizeof(V), sizeof(uint64_t)));
#endif  // ARCH_CPU_LITTLE_ENDIAN

    return true;
  }

 protected:
  std::map<uint64_t, uint64_t> values_;

 private:
  template <typename ULong>
  bool Read(pid_t pid);


  DISALLOW_COPY_AND_ASSIGN(AuxiliaryVector);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_LINUX_AUXILIARY_VECTOR_H_
