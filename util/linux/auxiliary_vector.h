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

#include <sys/types.h>

#include <unordered_map>

#include "base/macros.h"
#include "util/linux/address_types.h"

namespace crashpad {

//! \brief Read the auxiliary vector for a target thread.
template <typename ULong>
class AuxiliaryVector {
 public:
  AuxiliaryVector();
  ~AuxiliaryVector();

  //! \brief Initializes this object with the auxiliary vector for thread with
  //!     thread ID \a tid.
  //!
  //! This method must be called successfully prior to calling any other method
  //! in this class.
  //!
  //! \param[in] tid The thread ID of a target thread
  //!
  //! \return `true` on success, `false` on failure with a message logged.
  bool Initialize(pid_t tid);

  template <typename V>
  bool GetValue(ULong type, V* value) const {
    auto iter = values_.find(type);
    if (iter != values_.end()) {
      *value = static_cast<V>(iter->second);
      return true;
    }
    return false;
  }

 private:
  std::unordered_map<ULong, ULong> values_;

  DISALLOW_COPY_AND_ASSIGN(AuxiliaryVector<ULong>);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_LINUX_AUXILIARY_VECTOR_H_
