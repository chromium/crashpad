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

#include <errno.h>

#include "base/logging.h"

namespace crashpad {

DirectoryReader::DirectoryReader(const base::FilePath& path, bool* has_error)
    : filename_(),
      has_error_(has_error),
      find_data_(),
      handle_(),
      first_entry_(false) {
  if (path.empty()) {
    LOG(ERROR) << "Empty directory path";
    *has_error_ = true;
    return;
  }

  handle_.reset(FindFirstFile(
      path.Append(FILE_PATH_LITERAL("*")).value().c_str(), &find_data_));
  if (!handle_.is_valid()) {
    PLOG(ERROR) << "FindFirstFile";
    *has_error = true;
    return;
  }
  first_entry_ = true;
  *has_error_ = false;
}

DirectoryReader::~DirectoryReader() {}

DirectoryReader* DirectoryReader::NextFile() {
  if (!handle_.is_valid()) {
    return nullptr;
  }

  if (!first_entry_) {
    if (!FindNextFile(handle_.get(), &find_data_)) {
      if (GetLastError() != ERROR_NO_MORE_FILES) {
        PLOG(ERROR) << "FindNextFile";
        *has_error_ = true;
        return nullptr;
      }
      return nullptr;
    }
  } else {
    first_entry_ = false;
  }

  if (wcscmp(find_data_.cFileName, L".") == 0 ||
      wcscmp(find_data_.cFileName, L"..") == 0) {
    return NextFile();
  }

  filename_ = base::FilePath(find_data_.cFileName);
  return this;
}

}  // namespace crashpad
