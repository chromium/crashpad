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

#include "util/file/vmo_file_writer_fuchsia.h"

#include "base/logging.h"
#include "base/numerics/checked_math.h"

#include <stdio.h>

namespace crashpad {

VMOFileWriter::~VMOFileWriter() = default;

zx_status_t VMOFileWriter::GenerateVMO(zx::vmo* out) const {
  const std::string& data = string();

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
