// Copyright 2016 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_UTIL_FILE_READ_ENTIRE_FILE_H_
#define CRASHPAD_UTIL_FILE_READ_ENTIRE_FILE_H_

#include "base/files/file_path.h"

#include <string>
#include <vector>

namespace crashpad {

//! \brief Return values for the ReadEntireFile() and ReadOneLineFile() family
//!     of functions.
enum class ReadFileResult {
  //! \brief An error occurred calling `open()` or equivalent.
  //!
  //! No message is logged for this result, because it is reasonable for files
  //! to be missing or to not have permission to open them. When this value is
  //! returned, `errno` or `GetLastError()` will be set appropriately so that
  //! `PLOG()` may be used to log a message if desired.
  kOpenError = -2,

  //! \brief An error occurred calling `read()` or equivalent.
  //!
  //! A message is logged for this result.
  kReadError,

  //! \brief The file’s contents were not in the expected format.
  //!
  //! A message is logged for this result.
  kFormatError,

  //! \brief Success.
  //!
  //! The function’s out parameter is valid with this result.
  kSuccess,
};

//! \brief Reads an entire file into a string.
//!
//! \param[in] path The pathname of a file to read.
//! \param[out] contents The contents of the file.
//!
//! \return A ReadFileResult value.
ReadFileResult ReadEntireFile(const base::FilePath& path,
                              std::string* contents);

//! \brief Reads an entire file into a vector of strings, one per line.
//!
//! \param[in] path The pathname of a file to read.
//! \param[out] lines A vector whose elements are lines from the file, with
//!     newline characters removed.
//!
//! This function considers `"\n"` (`LF`) to be the only valid newline
//! character. It is considered a format error for a file with nonzero
//! content’s last line to not end with a newline terminator.
//!
//! \return A ReadFileResult value.
ReadFileResult ReadEntireFile(const base::FilePath& path,
                              std::vector<std::string>* lines);

//! \brief Reads a single-line file.
//!
//! \param[in] path The pathname of a file to read.
//! \param[out] value The value read from the file.
//!
//! The file must contain exactly one line.
//!
//! For numeric \a value types, StringToNumber() performs the value conversion.
//! It must be possible to convert the line to the desired type while remaining
//! in that type’s range.
//!
//! A file that does not meet these requirements will be treated as a format
//! error.
//!
//! \return A ReadFileResult value.
//!
//! \{
ReadFileResult ReadOneLineFile(const base::FilePath& path, std::string* value);

ReadFileResult ReadOneLineFile(const base::FilePath& path, char* value);
ReadFileResult ReadOneLineFile(const base::FilePath& path, signed char* value);
ReadFileResult ReadOneLineFile(const base::FilePath& path,
                               unsigned char* value);
ReadFileResult ReadOneLineFile(const base::FilePath& path, short* value);
ReadFileResult ReadOneLineFile(const base::FilePath& path,
                               unsigned short* value);
ReadFileResult ReadOneLineFile(const base::FilePath& path, int* value);
ReadFileResult ReadOneLineFile(const base::FilePath& path, unsigned int* value);
ReadFileResult ReadOneLineFile(const base::FilePath& path, long* value);
ReadFileResult ReadOneLineFile(const base::FilePath& path,
                               unsigned long* value);
ReadFileResult ReadOneLineFile(const base::FilePath& path, long long* value);
ReadFileResult ReadOneLineFile(const base::FilePath& path,
                               unsigned long long* value);
//! \}

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_FILE_READ_ENTIRE_FILE_H_
