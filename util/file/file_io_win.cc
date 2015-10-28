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

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/utf_string_conversions.h"

namespace {

bool IsSocketHandle(HANDLE file) {
  if (GetFileType(file) == FILE_TYPE_PIPE) {
    // FILE_TYPE_PIPE means that it's a socket, a named pipe, or an anonymous
    // pipe. If we are unable to retrieve the pipe information, we know it's a
    // socket.
    return !GetNamedPipeInfo(file, NULL, NULL, NULL, NULL);
  }
  return false;
}

}  // namespace

namespace crashpad {

namespace {

FileHandle OpenFileForOutput(DWORD access,
                             const base::FilePath& path,
                             FileWriteMode mode,
                             FilePermissions permissions) {
  DCHECK(access & GENERIC_WRITE);
  DCHECK_EQ(access & ~(GENERIC_READ | GENERIC_WRITE), 0u);

  DWORD disposition = 0;
  switch (mode) {
    case FileWriteMode::kReuseOrFail:
      disposition = OPEN_EXISTING;
      break;
    case FileWriteMode::kReuseOrCreate:
      disposition = OPEN_ALWAYS;
      break;
    case FileWriteMode::kTruncateOrCreate:
      disposition = CREATE_ALWAYS;
      break;
    case FileWriteMode::kCreateOrFail:
      disposition = CREATE_NEW;
      break;
  }
  return CreateFile(path.value().c_str(),
                    access,
                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                    nullptr,
                    disposition,
                    FILE_ATTRIBUTE_NORMAL,
                    nullptr);
}

}  // namespace

// TODO(scottmg): Handle > DWORD sized writes if necessary.

FileOperationResult ReadFile(FileHandle file, void* buffer, size_t size) {
  DCHECK(!IsSocketHandle(file));
  DWORD size_dword = base::checked_cast<DWORD>(size);
  DWORD total_read = 0;
  char* buffer_c = reinterpret_cast<char*>(buffer);
  while (size_dword > 0) {
    DWORD bytes_read;
    BOOL success = ::ReadFile(file, buffer_c, size_dword, &bytes_read, nullptr);
    if (!success) {
      if (GetLastError() == ERROR_BROKEN_PIPE) {
        // When reading a pipe and the write handle has been closed, ReadFile
        // fails with ERROR_BROKEN_PIPE, but only once all pending data has been
        // read.
        break;
      } else if (GetLastError() != ERROR_MORE_DATA) {
        return -1;
      }
    } else if (bytes_read == 0 && GetFileType(file) != FILE_TYPE_PIPE) {
      // Zero bytes read for a file indicates reaching EOF. Zero bytes read from
      // a pipe indicates only that there was a zero byte WriteFile issued on
      // the other end, so continue reading.
      break;
    }

    buffer_c += bytes_read;
    size_dword -= bytes_read;
    total_read += bytes_read;
  }
  return total_read;
}

FileOperationResult WriteFile(FileHandle file,
                              const void* buffer,
                              size_t size) {
  // TODO(scottmg): This might need to handle the limit for pipes across a
  // network in the future.
  DWORD size_dword = base::checked_cast<DWORD>(size);
  DWORD bytes_written;
  BOOL rv = ::WriteFile(file, buffer, size_dword, &bytes_written, nullptr);
  if (!rv)
    return -1;
  CHECK_EQ(bytes_written, size_dword);
  return bytes_written;
}

FileHandle OpenFileForRead(const base::FilePath& path) {
  return CreateFile(path.value().c_str(),
                    GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                    nullptr,
                    OPEN_EXISTING,
                    0,
                    nullptr);
}

FileHandle OpenFileForWrite(const base::FilePath& path,
                            FileWriteMode mode,
                            FilePermissions permissions) {
  return OpenFileForOutput(GENERIC_WRITE, path, mode, permissions);
}

FileHandle OpenFileForReadAndWrite(const base::FilePath& path,
                                   FileWriteMode mode,
                                   FilePermissions permissions) {
  return OpenFileForOutput(
      GENERIC_READ | GENERIC_WRITE, path, mode, permissions);
}

FileHandle LoggingOpenFileForRead(const base::FilePath& path) {
  FileHandle file = OpenFileForRead(path);
  PLOG_IF(ERROR, file == INVALID_HANDLE_VALUE)
      << "CreateFile " << base::UTF16ToUTF8(path.value());
  return file;
}

FileHandle LoggingOpenFileForWrite(const base::FilePath& path,
                                   FileWriteMode mode,
                                   FilePermissions permissions) {
  FileHandle file = OpenFileForWrite(path, mode, permissions);
  PLOG_IF(ERROR, file == INVALID_HANDLE_VALUE)
      << "CreateFile " << base::UTF16ToUTF8(path.value());
  return file;
}

FileHandle LoggingOpenFileForReadAndWrite(const base::FilePath& path,
                                          FileWriteMode mode,
                                          FilePermissions permissions) {
  FileHandle file = OpenFileForReadAndWrite(path, mode, permissions);
  PLOG_IF(ERROR, file == INVALID_HANDLE_VALUE)
      << "CreateFile " << base::UTF16ToUTF8(path.value());
  return file;
}

bool LoggingLockFile(FileHandle file, FileLocking locking) {
  DWORD flags =
      (locking == FileLocking::kExclusive) ? LOCKFILE_EXCLUSIVE_LOCK : 0;

  // Note that the `Offset` fields of overlapped indicate the start location for
  // locking (beginning of file in this case), and `hEvent` must be also be set
  // to 0.
  OVERLAPPED overlapped = {0};
  if (!LockFileEx(file, flags, 0, MAXDWORD, MAXDWORD, &overlapped)) {
    PLOG(ERROR) << "LockFileEx";
    return false;
  }
  return true;
}

bool LoggingUnlockFile(FileHandle file) {
  // Note that the `Offset` fields of overlapped indicate the start location for
  // locking (beginning of file in this case), and `hEvent` must be also be set
  // to 0.
  OVERLAPPED overlapped = {0};
  if (!UnlockFileEx(file, 0, MAXDWORD, MAXDWORD, &overlapped)) {
    PLOG(ERROR) << "UnlockFileEx";
    return false;
  }
  return true;
}

FileOffset LoggingSeekFile(FileHandle file, FileOffset offset, int whence) {
  DWORD method = 0;
  switch (whence) {
    case SEEK_SET:
      method = FILE_BEGIN;
      break;
    case SEEK_CUR:
      method = FILE_CURRENT;
      break;
    case SEEK_END:
      method = FILE_END;
      break;
    default:
      NOTREACHED();
      break;
  }

  LARGE_INTEGER distance_to_move;
  distance_to_move.QuadPart = offset;
  LARGE_INTEGER new_offset;
  BOOL result = SetFilePointerEx(file, distance_to_move, &new_offset, method);
  if (!result) {
    PLOG(ERROR) << "SetFilePointerEx";
    return -1;
  }
  return new_offset.QuadPart;
}

bool LoggingTruncateFile(FileHandle file) {
  if (LoggingSeekFile(file, 0, SEEK_SET) != 0)
    return false;
  if (!SetEndOfFile(file)) {
    PLOG(ERROR) << "SetEndOfFile";
    return false;
  }
  return true;
}

bool LoggingCloseFile(FileHandle file) {
  BOOL rv = CloseHandle(file);
  PLOG_IF(ERROR, !rv) << "CloseHandle";
  return !!rv;
}

}  // namespace crashpad
