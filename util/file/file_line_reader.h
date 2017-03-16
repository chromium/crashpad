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

#ifndef CRASHPAD_UTIL_FILE_FILE_LINE_READER_H_
#define CRASHPAD_UTIL_FILE_FILE_LINE_READER_H_

#include <string>

#include "base/macros.h"
#include "util/file/file_reader.h"

namespace crashpad {

//! \brief Reads a file one line at a time.
//!
//! This is a replacement for the standard library’s `getline()` function,
//! adapted to work with FileReaderInterface objects instead of `FILE*` streams.
class FileLineReader {
 public:
  //! \brief The result of a GetLine() call.
  enum class Result {
    //! \brief An error occurred, and a message was logged.
    kError = -1,

    //! \brief A line was read from the file.
    kSuccess,

    //! \brief The end of the file was encountered.
    kEndOfFile,
  };

  explicit FileLineReader(FileReaderInterface* file_reader);
  ~FileLineReader();

  //! \brief Reads a single line from the file.
  //!
  //! \param[out] line The line read from the file. This parameter will always
  //!     include the line’s terminating newline character unless the line was
  //!     at the end of the file and was read without such a character.
  //!
  //! \return a #Result value. \a line is only valid when Result::kSuccess is
  //!     returned.
  Result GetLine(std::string* line);

 private:
  char buf_[4096];
  FileReaderInterface* file_reader_;  // weak
  unsigned short buf_pos_;  // Index into buf_ of the start of the next line.
  unsigned short buf_len_;  // The size of buf_ that’s been filled.
  bool eof_;  // Caches the EOF signal when detected following a partial line.

  DISALLOW_COPY_AND_ASSIGN(FileLineReader);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_FILE_FILE_LINE_READER_H_
