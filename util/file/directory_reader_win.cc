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
#include "base/strings/utf_string_conversions.h"

namespace crashpad {

DirectoryReader::DirectoryReader()
    : find_data_(),
      handle_(),
      first_entry_(false) {}

DirectoryReader::~DirectoryReader() {}

bool DirectoryReader::Open(const base::FilePath& path) {
  const base::FilePath::StringType pattern =
      path.value() + FILE_PATH_LITERAL("\\*");
  LOG(INFO) << "Finding first file for " << base::UTF16ToUTF8(pattern.c_str());
  handle_.reset(FindFirstFile(pattern.c_str(), &find_data_));
  if (handle_.get() == INVALID_HANDLE_VALUE) {
    PLOG(ERROR) << "FindFirstFile";
    return false;
  }
  LOG(INFO) << "Found first file";
  first_entry_ = true;
  return true;
}

bool DirectoryReader::NextFile(base::FilePath* filename) {
  CHECK(handle_.is_valid());

  LOG(INFO) << "Finding file";
  if (!first_entry_) {
    if (!FindNextFile(handle_.get(), &find_data_)) {
      PLOG_IF(ERROR, GetLastError() != ERROR_NO_MORE_FILES) << "FindNextFile";
      return false;
    }
  }
  first_entry_ = false;

  *filename = base::FilePath(find_data_.cFileName);
  return true;
}

}  // namespace crashpad
