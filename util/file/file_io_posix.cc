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

#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <limits>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"

namespace {

constexpr size_t kMaxReadWriteSize =
    static_cast<size_t>(std::numeric_limits<ssize_t>::max());

}  // namespace

namespace crashpad {

namespace {

FileHandle OpenFileForOutput(int rdwr_or_wronly,
                             const base::FilePath& path,
                             FileWriteMode mode,
                             FilePermissions permissions) {
  int flags = O_NOCTTY | O_CLOEXEC;

  DCHECK(rdwr_or_wronly & (O_RDWR | O_WRONLY));
  DCHECK_EQ(rdwr_or_wronly & ~(O_RDWR | O_WRONLY), 0);
  flags |= rdwr_or_wronly;

  switch (mode) {
    case FileWriteMode::kReuseOrFail:
      break;
    case FileWriteMode::kReuseOrCreate:
      flags |= O_CREAT;
      break;
    case FileWriteMode::kTruncateOrCreate:
      flags |= O_CREAT | O_TRUNC;
      break;
    case FileWriteMode::kCreateOrFail:
      flags |= O_CREAT | O_EXCL;
      break;
  }

  return HANDLE_EINTR(
      open(path.value().c_str(),
           flags,
           permissions == FilePermissions::kWorldReadable ? 0644 : 0600));
}

}  // namespace

namespace internal {

const char kNativeReadFunctionName[] = "read";
const char kNativeWriteFunctionName[] = "write";

bool WriteFileInternal(FileHandle file,
                       const void* buffer,
                       size_t size,
                       size_t* bytes_written) {
  const size_t write_size = std::min(size, kMaxReadWriteSize);

  ssize_t rv = HANDLE_EINTR(write(file, buffer, write_size));
  if (rv < 0) {
    return false;
  }

  DCHECK_LE(static_cast<size_t>(rv), write_size);
  *bytes_written = rv;
  return true;
}

}  // namespace internal

FileOperationResult ReadFile(FileHandle file, void* buffer, size_t size) {
  const size_t read_size = std::min(size, kMaxReadWriteSize);

  FileOperationResult bytes_read = HANDLE_EINTR(read(file, buffer, read_size));
  if (bytes_read < 0) {
    return -1;
  }

  DCHECK_LE(static_cast<size_t>(bytes_read), read_size);
  return bytes_read;
}

FileHandle OpenFileForRead(const base::FilePath& path) {
  return HANDLE_EINTR(
      open(path.value().c_str(), O_RDONLY | O_NOCTTY | O_CLOEXEC));
}

FileHandle OpenFileForWrite(const base::FilePath& path,
                            FileWriteMode mode,
                            FilePermissions permissions) {
  return OpenFileForOutput(O_WRONLY, path, mode, permissions);
}

FileHandle OpenFileForReadAndWrite(const base::FilePath& path,
                                   FileWriteMode mode,
                                   FilePermissions permissions) {
  return OpenFileForOutput(O_RDWR, path, mode, permissions);
}

FileHandle LoggingOpenFileForRead(const base::FilePath& path) {
  FileHandle fd = OpenFileForRead(path);
  PLOG_IF(ERROR, fd < 0) << "open " << path.value();
  return fd;
}

FileHandle LoggingOpenFileForWrite(const base::FilePath& path,
                                   FileWriteMode mode,
                                   FilePermissions permissions) {
  FileHandle fd = OpenFileForWrite(path, mode, permissions);
  PLOG_IF(ERROR, fd < 0) << "open " << path.value();
  return fd;
}

FileHandle LoggingOpenFileForReadAndWrite(const base::FilePath& path,
                                          FileWriteMode mode,
                                          FilePermissions permissions) {
  FileHandle fd = OpenFileForReadAndWrite(path, mode, permissions);
  PLOG_IF(ERROR, fd < 0) << "open " << path.value();
  return fd;
}

bool LoggingLockFile(FileHandle file, FileLocking locking) {
  int operation = (locking == FileLocking::kShared) ? LOCK_SH : LOCK_EX;
  int rv = HANDLE_EINTR(flock(file, operation));
  PLOG_IF(ERROR, rv != 0) << "flock";
  return rv == 0;
}

bool LoggingUnlockFile(FileHandle file) {
  int rv = flock(file, LOCK_UN);
  PLOG_IF(ERROR, rv != 0) << "flock";
  return rv == 0;
}

FileOffset LoggingSeekFile(FileHandle file, FileOffset offset, int whence) {
  off_t rv = lseek(file, offset, whence);
  PLOG_IF(ERROR, rv < 0) << "lseek";
  return rv;
}

bool LoggingTruncateFile(FileHandle file) {
  if (HANDLE_EINTR(ftruncate(file, 0)) != 0) {
    PLOG(ERROR) << "ftruncate";
    return false;
  }
  return true;
}

bool LoggingCloseFile(FileHandle file) {
  int rv = IGNORE_EINTR(close(file));
  PLOG_IF(ERROR, rv != 0) << "close";
  return rv == 0;
}

FileOffset LoggingFileSizeByHandle(FileHandle file) {
  struct stat st;
  if (fstat(file, &st) != 0) {
    PLOG(ERROR) << "fstat";
    return -1;
  }
  return st.st_size;
}

FileHandle StdioFileHandle(StdioStream stdio_stream) {
  switch (stdio_stream) {
    case StdioStream::kStandardInput:
      return STDIN_FILENO;
    case StdioStream::kStandardOutput:
      return STDOUT_FILENO;
    case StdioStream::kStandardError:
      return STDERR_FILENO;
  }

  NOTREACHED();
  return kInvalidFileHandle;
}

}  // namespace crashpad
