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

#include "util/misc/paths.h"

#include "base/logging.h"

#include <sys/stat.h>
#include <zircon/process.h>
#include <zircon/syscalls.h>

namespace crashpad {

// static
bool Paths::Executable(base::FilePath* path) {
  // TODO(scottmg): ZX_MAX_NAME_LEN is way too short for this purpose (only 32
  // presently), but there's no other API to retrieve the name of the current
  // binary, so we try to make do with this for now. This will often result in
  // truncation. See https://crbug.com/726124 and ZX-797. For now, we use
  // zx_object_get_property with ZX_PROP_NAME, but fail if the returned path
  // doesn't exist (assuming truncation occurred).
  char name[ZX_MAX_NAME_LEN];
  if (zx_object_get_property(
          zx_process_self(), ZX_PROP_NAME, name, sizeof(name)) != ZX_OK) {
    return false;
  }

  struct stat buf;
  if (stat(name, &buf) == -1) {
    LOG(ERROR) << "Retrieved executable path '" << name << "' doesn't exist"
    return false;
  }

  *path = base::FilePath(name);

  return true;
}

}  // namespace crashpad
