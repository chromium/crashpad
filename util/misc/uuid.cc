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
#include "util/stdlib/cxx.h"

#if CXX_LIBRARY_VERSION >= 2011
#include <type_traits>
#endif

namespace crashpad {

#if CXX_LIBRARY_VERSION >= 2011
COMPILE_ASSERT(std::is_standard_layout<UUID>::value,
               UUID_must_be_standard_layout);
#endif

UUID::UUID() : data() {
}

UUID::UUID(const uint8_t* bytes) {
  memcpy(data, bytes, sizeof(data));
}

std::string UUID::ToString() const {
  return base::StringPrintf(
      "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      data[0],
      data[1],
      data[2],
      data[3],
      data[4],
      data[5],
      data[6],
      data[7],
      data[8],
      data[9],
      data[10],
      data[11],
      data[12],
      data[13],
      data[14],
      data[15]);
}

}  // namespace crashpad
