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

#include "util/file/directory.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/logging.h"

namespace crashpad {

bool IsDirectory(const base::FilePath& path) {
  struct stat st;
  if (stat(path.value().c_str(), &st) != 0) {
    PLOG(ERROR) << "stat " << path.value();
    return false;
  }
  if (!S_ISDIR(st.st_mode)) {
    LOG(ERROR) << "stat " << path.value() << ": not a directory";
    return false;
  }
  return true;
}

bool LoggingCreateDirectory(const base::FilePath& path, bool may_reuse) {
  if (mkdir(path.value().c_str(), 0755) == 0) {
    return true;
  }
  if (may_reuse && errno == EEXIST) {
    return IsDirectory(path);
  }
  PLOG(ERROR) << "mkdir " << path.value();
  return false;
}

bool LoggingRemoveDirectory(const base::FilePath& path) {
  if (rmdir(path.value().c_str()) != 0) {
    PLOG(ERROR) << "rmdir " << path.value();
    return false;
  }
  return true;
}

}  // namespace crashpad
