// Copyright 2014 The Crashpad Authors. All rights reserved.
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

#include "util/misc/uuid.h"

#include <string.h>

#include "base/basictypes.h"
#include "base/strings/stringprintf.h"
#include "base/sys_byteorder.h"
#include "util/stdlib/cxx.h"

#if CXX_LIBRARY_VERSION >= 2011
#include <type_traits>
#endif

namespace crashpad {

COMPILE_ASSERT(sizeof(UUID) == 16, UUID_must_be_16_bytes);

#if CXX_LIBRARY_VERSION >= 2011
COMPILE_ASSERT(std::is_standard_layout<UUID>::value,
               UUID_must_be_standard_layout);
#endif

UUID::UUID() : data_1(0), data_2(0), data_3(0), data_4(), data_5() {
}

UUID::UUID(const uint8_t* bytes) {
  InitializeFromBytes(bytes);
}

void UUID::InitializeFromBytes(const uint8_t* bytes) {
  memcpy(this, bytes, sizeof(*this));
  data_1 = base::NetToHost32(data_1);
  data_2 = base::NetToHost16(data_2);
  data_3 = base::NetToHost16(data_3);
}

std::string UUID::ToString() const {
  return base::StringPrintf("%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                            data_1,
                            data_2,
                            data_3,
                            data_4[0],
                            data_4[1],
                            data_5[0],
                            data_5[1],
                            data_5[2],
                            data_5[3],
                            data_5[4],
                            data_5[5]);
}

}  // namespace crashpad
