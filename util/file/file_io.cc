// Copyright 2014 The Crashpad Authors. All rights reserved.
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

#include "util/file/file_io.h"

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"

namespace crashpad {

bool LoggingReadFile(FileHandle file, void* buffer, size_t size) {
  FileOperationResult expect = base::checked_cast<FileOperationResult>(size);
  FileOperationResult rv = ReadFile(file, buffer, size);
  if (rv < 0) {
    PLOG(ERROR) << "read";
    return false;
  }
  if (rv != expect) {
    LOG(ERROR) << "read: expected " << expect << ", observed " << rv;
    return false;
  }

  return true;
}

bool LoggingWriteFile(FileHandle file, const void* buffer, size_t size) {
  FileOperationResult expect = base::checked_cast<FileOperationResult>(size);
  FileOperationResult rv = WriteFile(file, buffer, size);
  if (rv < 0) {
    PLOG(ERROR) << "write";
    return false;
  }
  if (rv != expect) {
    LOG(ERROR) << "write: expected " << expect << ", observed " << rv;
    return false;
  }

  return true;
}

void CheckedReadFile(FileHandle file, void* buffer, size_t size) {
  CHECK(LoggingReadFile(file, buffer, size));
}

void CheckedWriteFile(FileHandle file, const void* buffer, size_t size) {
  CHECK(LoggingWriteFile(file, buffer, size));
}

void CheckedReadFileAtEOF(FileHandle file) {
  char c;
  FileOperationResult rv = ReadFile(file, &c, 1);
  if (rv < 0) {
    PCHECK(rv == 0) << "read";
  } else {
    CHECK_EQ(rv, 0) << "read";
  }
}

void CheckedCloseFile(FileHandle file) {
  CHECK(LoggingCloseFile(file));
}

}  // namespace crashpad
