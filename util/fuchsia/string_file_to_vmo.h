// Copyright 2019 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_UTIL_FUCHSIA_STRING_FILE_TO_VMO_H_
#define CRASHPAD_UTIL_FUCHSIA_STRING_FILE_TO_VMO_H_

#include <lib/zx/vmo.h>

#include "util/file/string_file.h"

namespace crashpad {

  // \brief Generates a Fuchsia vmo the current content of the FileWriter.
  //
  // \return `ZX_OK` if operation succeeded. Returns the correspondent
  //     zx_status_t error otherwise and have the out vmo hold the data.
  //     One error a message is logged and the out vmo will be untouched.
  //
  // \note This does not invalidate the contents within the FileWriter.
  //     This means that users can continue to then append data to this
  //     FileWriter and then generate another vmo.
zx_status_t GenerateVMOFromStringFile(const StringFile& string_file,
                                      zx::vmo* out);

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_FUCHSIA_STRING_FILE_TO_VMO_H_
