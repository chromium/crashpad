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
#include "test/main_arguments.h"

namespace crashpad {

// static
bool Paths::Executable(base::FilePath* path) {
  // TODO(scottmg): It would be nice to get the canonical executable path
  // from a kernel API. See https://crbug.com/726124, and ZX-797.
  // For now, we reach into the test code saving of the executable name.

  LOG(ERROR) << "Incomplete and incorrect Paths::Executable implementation.";

  const auto& argv = crashpad::test::GetMainArguments();
  if (argv.empty())
    return false;

  char executable_path[PATH_MAX + 1];
  if (realpath(argv[0].c_str(), executable_path)) {
    *path = base::FilePath(executable_path);
  } else {
    *path = base::FilePath(argv[0]);
  }

  return true;
}

}  // namespace crashpad
