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

#include "util/fuchsia/string_file_to_vmo.h"

#include "base/logging.h"

namespace crashpad {

zx_status_t GenerateVMOFromStringFile(const StringFile& string_file,
                                      zx::vmo* out) {
  const std::string& data = string_file.string();
  if (data.empty())
    return ZX_ERR_INVALID_ARGS;

  zx::vmo vmo;
  zx_status_t status = zx::vmo::create(data.size(), 0, &vmo);
  if (status != ZX_OK) {
    PLOG(ERROR) << "Could create VMO.";
    return status;
  }

  // Write the data into the vmo.
  status = vmo.write(data.data(), 0, data.size());
  if (status != ZX_OK) {
    PLOG(ERROR) << "Could not write into the VMO.";
    return status;
  }

  *out = std::move(vmo);
  return ZX_OK;
}

}  // namespace crashpad
