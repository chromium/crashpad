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

#include "test/paths.h"

#include <mach-o/dyld.h>
#include <stdint.h>

#include "base/logging.h"

namespace crashpad {
namespace test {

// static
base::FilePath Paths::Executable() {
  uint32_t executable_length = 0;
  _NSGetExecutablePath(nullptr, &executable_length);
  CHECK_GT(executable_length, 1u);

  std::string executable_path(executable_length - 1, std::string::value_type());
  int rv = _NSGetExecutablePath(&executable_path[0], &executable_length);
  CHECK_EQ(rv, 0);

  return base::FilePath(executable_path);
}

}  // namespace test
}  // namespace crashpad
