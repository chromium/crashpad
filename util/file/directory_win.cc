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

#include <windows.h>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"

namespace crashpad {

bool IsDirectory(const base::FilePath& path) {
  DWORD fileattr = GetFileAttributes(path.value().c_str());
  if (fileattr == INVALID_FILE_ATTRIBUTES) {
    PLOG(ERROR) << "GetFileAttributes " << base::UTF16ToUTF8(path.value());
    return false;
  }
  if ((fileattr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
    return false;
  }
  return true;
}

bool LoggingCreateDirectory(const base::FilePath& path, bool may_reuse) {
  if (CreateDirectory(path.value().c_str(), nullptr)) {
    return true;
  }
  if (may_reuse && GetLastError() == ERROR_ALREADY_EXISTS) {
    if (!IsDirectory(path)) {
      LOG(ERROR) << base::UTF16ToUTF8(path.value()) << " not a directory";
      return false;
    }
    return true;
  }
  PLOG(ERROR) << "CreateDirectory " << base::UTF16ToUTF8(path.value());
  return false;
}

bool LoggingRemoveDirectory(const base::FilePath& path) {
  if (!RemoveDirectory(path.value().c_str())) {
    PLOG(ERROR) << "RemoveDirectory" << base::UTF16ToUTF8(path.value());
    return false;
  }
  return true;
}

}  // namespace crashpad
