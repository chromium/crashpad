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

#include "util/file/directory_reader.h"

#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

#include "base/logging.h"

namespace crashpad {

#define HANDLE_EINTR_IF_EQ(x, val)                           \
  ({                                                         \
    decltype(x) eintr_wrapper_result;                        \
    do {                                                     \
      eintr_wrapper_result = (x);                            \
    } while (eintr_wrapper_result == val && errno == EINTR); \
    eintr_wrapper_result;                                    \
  })

DirectoryReader::DirectoryReader(const base::FilePath& path, bool* error)
    : filename_(), error_(error), dir_(), no_more_files_(false) {
  if (error_) {
    *error_ = false;
  }

  dir_.reset(HANDLE_EINTR_IF_EQ(opendir(path.value().c_str()), nullptr));
  if (!dir_.is_valid()) {
    PLOG(ERROR) << "opendir";
    SetError();
    return;
  }

  NextFile();
}

bool DirectoryReader::NextFile() {
  if (no_more_files_) {
    return false;
  }
  DCHECK(dir_.is_valid());

  errno = 0;
  dirent* entry = HANDLE_EINTR_IF_EQ(readdir(dir_.get()), nullptr);
  if (!entry) {
    if (errno) {
      PLOG(ERROR) << "readdir";
      SetError();
    } else {
      SetToEnd();
    }
    return false;
  }

  if (strncmp(entry->d_name, ".", arraysize(entry->d_name)) == 0 ||
      strncmp(entry->d_name, "..", arraysize(entry->d_name)) == 0) {
    return NextFile();
  }

  filename_ = base::FilePath(entry->d_name);
  return true;
}

int DirectoryReader::DirectoryFD() {
  DCHECK(dir_.is_valid());
  int rv = dirfd(dir_.get());
  if (rv < 0) {
    PLOG(ERROR) << "dirfd";
    SetError();
  }
  return rv;
}

}  // namespace crashpad
