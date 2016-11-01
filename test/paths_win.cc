// Copyright 2015 The Crashpad Authors. All rights reserved.
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

#include <windows.h>

#include "base/logging.h"

namespace crashpad {
namespace test {

// static
base::FilePath Paths::Executable() {
  wchar_t executable_path[_MAX_PATH];
  unsigned int len =
      GetModuleFileName(nullptr, executable_path, arraysize(executable_path));
  PCHECK(len != 0 && len < arraysize(executable_path)) << "GetModuleFileName";
  return base::FilePath(executable_path);
}

}  // namespace test
}  // namespace crashpad
