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

#ifndef CRASHPAD_UTIL_FILE_DIRECTORY_READER_H_
#define CRASHPAD_UTIL_FILE_DIRECTORY_READER_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "build/build_config.h"

#if defined(OS_POSIX)
#include "util/posix/scoped_dir.h"
#elif defined(OS_WIN)
#include <windows.h>

#include "util/win/scoped_handle.h"
#endif  // OS_POSIX

namespace crashpad {

//! \brief Enumerates files in a directory.
class DirectoryReader {
 public:
  //! \brief Result codes returned by NextFile().
  enum class Result {
    //! \brief An error occurred.
    kError = -1,

    //! \brief A file was found.
    kFileFound,

    //! \brief No more files were found.
    kFileNotFound
  };

  DirectoryReader();
  ~DirectoryReader();

  //! \brief Opens the directory specified by \a path for reading.
  //!
  //! \return `true` on success. `false` on failure with a message logged.
  bool Open(const base::FilePath& path);

  //! \brief Find the next file in the directory.
  //!
  //! \param[out] filename The filename of the next file, if found.
  //! \return a #Result value. \a filename is only valid if Result::kFileFound
  //!     is returned. If Result::kError is returned, a message is logged.
  Result NextFile(base::FilePath* filename);

 private:
#if defined(OS_POSIX)
  ScopedDIR dir_;
#elif defined(OS_WIN)
  WIN32_FIND_DATA find_data_;
  ScopedSearchHANDLE handle_;
  bool first_entry_;
#endif  // OS_POSIX

  DISALLOW_COPY_AND_ASSIGN(DirectoryReader);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_FILE_DIRECTORY_READER_H_
