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
#include <sys/types.h>

#include "base/logging.h"

namespace crashpad {

DirectoryReader::DirectoryReader() {}

DirectoryReader::~DirectoryReader() {}

bool DirectoryReader::Open(const base::FilePath& path) {
  dir_.reset(opendir(path.value().c_str()));
  if (!dir_.is_valid()) {
    PLOG(ERROR) << "opendir";
    return false;
  }
  return true;
}

DirectoryReader::Result DirectoryReader::NextFile(base::FilePath* filename) {
  CHECK(dir_.is_valid());

  errno = 0;
  dirent* entry = readdir(dir_.get());
  if (!entry) {
    if (errno) {
      PLOG(ERROR) << "readdir";
      return Result::kError;
    }
    return Result::kFileNotFound;
  }

  *filename = base::FilePath(entry->d_name);
  return Result::kFileFound;
}

}  // namespace crashpad
