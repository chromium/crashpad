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

DirectoryReader::DirectoryReader(const base::FilePath& path, bool* has_error)
    : filename_(), has_error_(has_error), dir_() {
  dir_.reset(opendir(path.value().c_str()));
  if (!dir_.is_valid()) {
    PLOG(ERROR) << "opendir";
    *has_error_ = true;
    return;
  }
  *has_error_ = false;
}

DirectoryReader::~DirectoryReader() {}

DirectoryReader* DirectoryReader::NextFile() {
  if (!dir_.is_valid()) {
    return nullptr;
  }

  errno = 0;
  dirent* entry = readdir(dir_.get());
  if (!entry) {
    if (errno) {
      PLOG(ERROR) << "readdir";
      *has_error_ = true;
    }
    return nullptr;
  }

  if (strncmp(entry->d_name, ".", arraysize(entry->d_name)) == 0 ||
      strncmp(entry->d_name, "..", arraysize(entry->d_name)) == 0) {
    return NextFile();
  }

  filename_ = base::FilePath(entry->d_name);
  return this;
}

}  // namespace crashpad
