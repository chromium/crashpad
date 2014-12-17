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

#ifndef CRASHPAD_UTIL_FILE_FILE_IO_H_
#define CRASHPAD_UTIL_FILE_FILE_IO_H_

#include <sys/types.h>

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace crashpad {

#if defined(OS_POSIX)
using FileHandle = int;
#elif defined(OS_WIN)
using FileHandle = HANDLE;
#endif

//! \brief Reads from a file, retrying when interrupted on POSIX or following a
//!        short read.
//!
//! This function reads into \a buffer, stopping only when \a size bytes have
//! been read or when end-of-file has been reached. On Windows, reading from
//! sockets is not currently supported.
//!
//! \return The number of bytes read and placed into \a buffer, or `-1` on
//!     error, with `errno` or `GetLastError()` set appropriately. On error, a
//!     portion of \a file may have been read into \a buffer.
//!
//! \sa WriteFile
//! \sa LoggingReadFile
//! \sa CheckedReadFile
//! \sa CheckedReadFileAtEOF
ssize_t ReadFile(FileHandle file, void* buffer, size_t size);

//! \brief Writes to a file, retrying when interrupted or following a short
//!        write on POSIX.
//!
//! This function writes to \a file, stopping only when \a size bytes have been
//! written.
//!
//! \return The number of bytes written from \a buffer, or `-1` on error, with
//!     `errno` or `GetLastError()` set appropriately. On error, a portion of
//!     \a buffer may have been written to \a file.
//!
//! \sa ReadFile
//! \sa LoggingWriteFile
//! \sa CheckedWriteFile
ssize_t WriteFile(FileHandle file, const void* buffer, size_t size);

//! \brief Wraps ReadFile(), ensuring that exactly \a size bytes are read.
//!
//! \return `true` on success. If \a size is out of the range of possible
//!     ReadFile() return values, if the underlying ReadFile() fails, or if
//!     other than \a size bytes were read, this function logs a message and
//!     returns `false`.
//!
//! \sa LoggingWriteFile
//! \sa ReadFile
//! \sa CheckedReadFile
//! \sa CheckedReadFileAtEOF
bool LoggingReadFile(FileHandle file, void* buffer, size_t size);

//! \brief Wraps WriteFile(), ensuring that exactly \a size bytes are written.
//!
//! \return `true` on success. If \a size is out of the range of possible
//!     WriteFile() return values, if the underlying WriteFile() fails, or if
//!     other than \a size bytes were written, this function logs a message and
//!     returns `false`.
//!
//! \sa LoggingReadFile
//! \sa WriteFile
//! \sa CheckedWriteFile
bool LoggingWriteFile(FileHandle file, const void* buffer, size_t size);

//! \brief Wraps ReadFile(), ensuring that exactly \a size bytes are read.
//!
//! If \a size is out of the range of possible ReadFile() return values, if the
//! underlying ReadFile() fails, or if other than \a size bytes were read, this
//! function causes execution to terminate without returning.
//!
//! \sa CheckedWriteFile
//! \sa ReadFile
//! \sa LoggingReadFile
//! \sa CheckedReadFileAtEOF
void CheckedReadFile(FileHandle file, void* buffer, size_t size);

//! \brief Wraps WriteFile(), ensuring that exactly \a size bytes are written.
//!
//! If \a size is out of the range of possible WriteFile() return values, if the
//! underlying WriteFile() fails, or if other than \a size bytes were written,
//! this function causes execution to terminate without returning.
//!
//! \sa CheckedReadFile
//! \sa WriteFile
//! \sa LoggingWriteFile
void CheckedWriteFile(FileHandle file, const void* buffer, size_t size);

//! \brief Wraps ReadFile(), ensuring that it indicates end-of-file.
//!
//! Attempts to read a single byte from \a file, expecting no data to be read.
//! If the underlying ReadFile() fails, or if a byte actually is read, this
//! function causes execution to terminate without returning.
//!
//! \sa CheckedReadFile
//! \sa ReadFile
void CheckedReadFileAtEOF(FileHandle file);

//! \brief Wraps `close()` or `CloseHandle()`, logging an error if the operation
//!        fails.
//!
//! \return On success, `true` is returned. On failure, an error is logged and
//!     `false` is returned.
bool LoggingCloseFile(FileHandle file);

//! \brief Wraps `close()` or `CloseHandle()`, ensuring that it succeeds.
//!
//! If the underlying function fails, this function causes execution to
//! terminate without returning.
void CheckedCloseFile(FileHandle file);

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_FILE_FILE_IO_H_
