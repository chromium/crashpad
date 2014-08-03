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

#include "util/test/errors.h"

#include <errno.h>

#include "base/safe_strerror_posix.h"
#include "base/strings/stringprintf.h"

namespace crashpad {
namespace test {

std::string ErrnoMessage(int err, const std::string& base) {
  return base::StringPrintf("%s%s%s (%d)",
                            base.c_str(),
                            base.empty() ? "" : ": ",
                            safe_strerror(errno).c_str(),
                            err);
}

std::string ErrnoMessage(const std::string& base) {
  return ErrnoMessage(errno, base);
}

}  // namespace test
}  // namespace crashpad
