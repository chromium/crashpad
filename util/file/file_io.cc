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

namespace {

bool ReadFileExactlyInternal(FileHandle file,
                             void* buffer,
                             size_t size,
                             bool can_log) {
  FileOperationResult expect = base::checked_cast<FileOperationResult>(size);
  char* buffer_c = static_cast<char*>(buffer);

  FileOperationResult total_bytes = 0;
  while (size > 0) {
    FileOperationResult bytes = ReadFile(file, buffer, size);
    if (bytes < 0) {
      PLOG_IF(ERROR, can_log) << kNativeReadFunctionName;
      return false;
    }

    DCHECK_LE(static_cast<size_t>(bytes), size);

    if (bytes == 0) {
      break;
    }

    buffer_c += bytes;
    size -= bytes;
    total_bytes += bytes;
  }

  if (total_bytes != expect) {
    LOG_IF(ERROR, can_log) << kNativeReadFunctionName << ": expected " << expect
                           << ", observed " << total_bytes;
    return false;
  }

  return true;
}

}  // namespace

bool ReadFileExactly(FileHandle file, void* buffer, size_t size) {
  return ReadFileExactlyInternal(file, buffer, size, false);
}

bool LoggingReadFileExactly(FileHandle file, void* buffer, size_t size) {
  return ReadFileExactlyInternal(file, buffer, size, true);
}

bool LoggingWriteFile(FileHandle file, const void* buffer, size_t size) {
  FileOperationResult expect = base::checked_cast<FileOperationResult>(size);
  FileOperationResult rv = WriteFile(file, buffer, size);
  if (rv < 0) {
    PLOG(ERROR) << kNativeWriteFunctionName;
    return false;
  }
  if (rv != expect) {
    LOG(ERROR) << kNativeWriteFunctionName << ": expected " << expect
               << ", observed " << rv;
    return false;
  }

  return true;
}

void CheckedReadFileExactly(FileHandle file, void* buffer, size_t size) {
  CHECK(LoggingReadFileExactly(file, buffer, size));
}

void CheckedWriteFile(FileHandle file, const void* buffer, size_t size) {
  CHECK(LoggingWriteFile(file, buffer, size));
}

void CheckedReadFileAtEOF(FileHandle file) {
  char c;
  FileOperationResult rv = ReadFile(file, &c, 1);
  if (rv < 0) {
    PCHECK(rv == 0) << kNativeReadFunctionName;
  } else {
    CHECK_EQ(rv, 0) << kNativeReadFunctionName;
  }
}

void CheckedCloseFile(FileHandle file) {
  CHECK(LoggingCloseFile(file));
}

}  // namespace crashpad
