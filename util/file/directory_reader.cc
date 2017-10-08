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

#include "base/logging.h"

namespace crashpad {

DirectoryReader::~DirectoryReader() {}

DirectoryReader::Iterator::Iterator(DirectoryReader* reader)
    : reader_(reader) {}

DirectoryReader::Iterator::~Iterator() {}

bool DirectoryReader::Iterator::operator!=(const Iterator& other) const {
  return reader_ != other.reader_;
}

bool DirectoryReader::Iterator::operator==(const Iterator& other) const {
  return reader_ == other.reader_;
}

DirectoryReader::Iterator& DirectoryReader::Iterator::operator++() {
  DCHECK(reader_);
  if (!reader_->NextFile()) {
    reader_ = nullptr;
  }
  return *this;
}

const base::FilePath& DirectoryReader::Iterator::operator*() const {
  DCHECK(reader_);
  return reader_->Filename();
}

DirectoryReader::Iterator DirectoryReader::begin() {
  return no_more_files_ ? end() : Iterator(this);
}

// static
DirectoryReader::Iterator DirectoryReader::end() {
  return Iterator(nullptr);
}

void DirectoryReader::SetToEnd() {
  no_more_files_ = true;
  filename_ = base::FilePath();
}

void DirectoryReader::SetError() {
  SetToEnd();
  if (error_) {
    *error_ = true;
  }
}

}  // namespace crashpad
