// Copyright 2015 The Crashpad Authors. All rights reserved.
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

#include "util/file/file_reader.h"

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"

namespace crashpad {

bool FileReaderInterface::ReadExactly(void* data, size_t size) {
  ssize_t expect = base::checked_cast<ssize_t>(size);
  ssize_t rv = Read(data, size);
  if (rv < 0) {
    // Read() will have logged its own error.
    return false;
  } else if (rv != expect) {
    LOG(ERROR) << "ReadExactly(): expected " << expect << ", observed " << rv;
    return false;
  }

  return true;
}

WeakFileHandleFileReader::WeakFileHandleFileReader(FileHandle file_handle)
    : file_handle_(file_handle) {
}

WeakFileHandleFileReader::~WeakFileHandleFileReader() {
}

ssize_t WeakFileHandleFileReader::Read(void* data, size_t size) {
  DCHECK_NE(file_handle_, kInvalidFileHandle);

  // Don’t use LoggingReadFile(), which insists on a full read and only returns
  // a bool. This method permits short reads and returns the number of bytes
  // read.
  base::checked_cast<ssize_t>(size);
  ssize_t rv = ReadFile(file_handle_, data, size);
  if (rv < 0) {
    PLOG(ERROR) << "read";
    return -1;
  }

  return rv;
}

FileOffset WeakFileHandleFileReader::Seek(FileOffset offset, int whence) {
  DCHECK_NE(file_handle_, kInvalidFileHandle);
  return LoggingSeekFile(file_handle_, offset, whence);
}

FileReader::FileReader()
    : file_(),
      weak_file_handle_file_reader_(kInvalidFileHandle) {
}

FileReader::~FileReader() {
}

bool FileReader::Open(const base::FilePath& path,
                      FileWriteMode write_mode,
                      FilePermissions permissions) {
  CHECK(!file_.is_valid());
  file_.reset(LoggingOpenFileForRead(path));
  if (!file_.is_valid()) {
    return false;
  }

  weak_file_handle_file_reader_.set_file_handle(file_.get());
  return true;
}

void FileReader::Close() {
  CHECK(file_.is_valid());

  weak_file_handle_file_reader_.set_file_handle(kInvalidFileHandle);
  file_.reset();
}

ssize_t FileReader::Read(void* data, size_t size) {
  DCHECK(file_.is_valid());
  return weak_file_handle_file_reader_.Read(data, size);
}

FileOffset FileReader::Seek(FileOffset offset, int whence) {
  DCHECK(file_.is_valid());
  return weak_file_handle_file_reader_.Seek(offset, whence);
}

}  // namespace crashpad
