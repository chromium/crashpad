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

#include "util/linux/auxiliary_vector.h"

#include "util/file/file_reader.h"

namespace crashpad {

template <typename ULong>
AuxiliaryVector<ULong>::AuxiliaryVector() : values_() {}

template <typename ULong>
AuxiliaryVector<ULong>::~AuxiliaryVector() {}

template <typename ULong>
bool AuxiliaryVector<ULong>::Initialize(pid_t tid) {
  char path[32];
  snprintf(path, sizeof(path), "/proc/%d/auxv", tid);
  FileReader aux_file;
  if (!aux_file.Open(base::FilePath(path))) {
    return false;
  }

  ULong type;
  ULong value;
  while (aux_file.ReadExactly(&type, sizeof(type)) &&
         aux_file.ReadExactly(&value, sizeof(value))) {
    if (type == 0 && value == 0) {
      return true;
    }
    values_[type] = value;
  }
  return false;
}

template class AuxiliaryVector<uint32_t>;
template class AuxiliaryVector<uint64_t>;

}  // namespace crashpad
