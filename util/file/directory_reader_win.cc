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

DirectoryReader::DirectoryReader(const base::FilePath& path, bool* error)
    : filename_(), error_(error), find_data_(), handle_(), first_entry_(false) {
  if (path.empty()) {
    LOG(ERROR) << "Empty directory path";
    SetError(true);
    return;
  }

  LOG(INFO) << "Finding first file";
  handle_.reset(
      FindFirstFileEx(path.Append(FILE_PATH_LITERAL("*")).value().c_str(),
                      FindExInfoBasic,
                      &find_data_,
                      FindExSearchNameMatch,
                      nullptr,
                      FIND_FIRST_EX_LARGE_FETCH));
  LOG(INFO) << "Done";
  if (!handle_.is_valid()) {
    PLOG(ERROR) << "FindFirstFile";
    SetError(true);
    return;
  }
  first_entry_ = true;
  SetError(false);
}

DirectoryReader::~DirectoryReader() {}

DirectoryReader* DirectoryReader::NextFile() {
  if (!handle_.is_valid()) {
    return nullptr;
  }

  if (!first_entry_) {
    LOG(INFO) << "Finding next file";
    if (!FindNextFile(handle_.get(), &find_data_)) {
      if (GetLastError() != ERROR_NO_MORE_FILES) {
        PLOG(ERROR) << "FindNextFile";
        SetError(true);
        return nullptr;
      }
      return nullptr;
    }
  } else {
    first_entry_ = false;
  }

  LOG(INFO) << "Checking for . or ..";
  if (wcscmp(find_data_.cFileName, L".") == 0 ||
      wcscmp(find_data_.cFileName, L"..") == 0) {
    return NextFile();
  }

  LOG(INFO) << "file found";
  filename_ = base::FilePath(find_data_.cFileName);
  return this;
}

}  // namespace crashpad
