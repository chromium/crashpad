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

#include <linux/auxvec.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "util/file/file_reader.h"
#include "util/stdlib/map_insert.h"

namespace crashpad {

AuxiliaryVector::AuxiliaryVector() : values_() {}

AuxiliaryVector::~AuxiliaryVector() {}

bool AuxiliaryVector::Initialize(pid_t pid, bool is_64_bit) {
  return is_64_bit ? Read<uint64_t>(pid) : Read<uint32_t>(pid);
}

template <typename ULong>
bool AuxiliaryVector::Read(pid_t pid) {
  char path[32];
  snprintf(path, sizeof(path), "/proc/%d/auxv", pid);
  FileReader aux_file;
  if (!aux_file.Open(base::FilePath(path))) {
    return false;
  }

  ULong type;
  ULong value;
  while (aux_file.ReadExactly(&type, sizeof(type)) &&
         aux_file.ReadExactly(&value, sizeof(value))) {
    if (type == AT_NULL && value == 0) {
      return true;
    }
    if (type == AT_IGNORE) {
      continue;
    }
    if (!MapInsertOrReplace(&values_, type, value, nullptr)) {
      LOG(ERROR) << "duplicate auxv entry";
      return false;
    }
  }
  return false;
}

// static
bool AuxiliaryVector::VariableSizeBitCast(uint64_t data,
                                          char* dest,
                                          size_t dest_size) {
  auto data_p = reinterpret_cast<const char*>(&data);
  constexpr size_t data_size = sizeof(uint64_t);

  // Verify that any unused bytes from data are zero.
  // The unused bytes are at the start of the data buffer for big-endian and the
  // end of the buffer for little-endian.
  if (dest_size < data_size) {
    uint64_t zero = 0;
    auto extra_bytes = data_p;
#if defined(ARCH_CPU_LITTLE_ENDIAN)
    extra_bytes += dest_size;
#endif  // ARCH_CPU_LITTLE_ENDIAN
    if (memcmp(extra_bytes, &zero, data_size - dest_size) != 0) {
      LOG(ERROR) << "information loss";
      return false;
    }
  }

  // Zero out the destination, in case it is larger than data.
  memset(dest, 0, dest_size);

#if defined(ARCH_CPU_LITTLE_ENDIAN)
  // Copy a prefix of data to a prefix of dest for little-endian
  memcpy(dest, data_p, std::min(dest_size, data_size));
#else
  // or the suffix of data to the suffix of dest for big-endian
  if (data_size >= dest_size) {
    memcpy(dest, data_p + data_size - dest_size, dest_size);
  } else {
    memcpy(dest + dest_size - data_size, data_p, data_size);
  }
#endif  // ARCH_CPU_LITTLE_ENDIAN
  return true;
}

}  // namespace crashpad
