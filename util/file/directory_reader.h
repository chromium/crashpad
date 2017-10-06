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
#include "base/logging.h"
#include "base/macros.h"
#include "build/build_config.h"

#if defined(OS_POSIX)
#include "util/posix/scoped_dir.h"
#elif defined(OS_WIN)
#include <windows.h>

#include "util/win/scoped_handle.h"
#endif  // OS_POSIX

namespace crashpad {

//! \brief Iterates over the file and directory names in a directory.
//!
//! The names enumerated are relative to the specified directory and do not
//! include ".", "..", or files and directories in subdirectories.
class DirectoryReader {
 public:
  //! \brief Opens the directory specified by \a path for reading.
  //!
  //! \param[in] path The path to the directory to read.
  //! \param[out] error Whether an error has occurred. If \a error is not
  //!     `nullptr` it is set to `true` if an error occurs when calling any
  //!     method in this class. Otherwise, it is set to `false`.
  explicit DirectoryReader(const base::FilePath& path, bool* error = nullptr);

  ~DirectoryReader();

  //! \brief Advances the reader to the next file in the directory.
  //!
  //! \return A pointer to this object on success, or `nullptr` if there are no
  //!     more files or an error occurred. If an error occurs, a message will be
  //!     logged.
  DirectoryReader* NextFile();

  //! \brief Return the filename of the current file.
  const base::FilePath& Filename() { return filename_; }

#if defined(OS_POSIX)
  int DirectoryFD() const;
#endif

 private:
  // Range-based for loop support
  class Iterator {
   public:
    Iterator(DirectoryReader* reader) : reader_(reader) {}
    ~Iterator() {}

    bool operator!=(const Iterator& other) const {
      return reader_ != other.reader_;
    }
    bool operator==(const Iterator& other) const {
      return reader_ == other.reader_;
    }
    Iterator& operator++() {
      DCHECK(reader_);
      reader_ = reader_->NextFile();
      return *this;
    }
    const base::FilePath& operator*() {
      DCHECK(reader_);
      return reader_->Filename();
    }

   private:
    DirectoryReader* reader_;
  };

 public:
  Iterator begin() { return Iterator(this->NextFile()); }
  static Iterator end() { return Iterator(nullptr); }

 private:
  void SetError(bool val) {
    if (error_)
      *error_ = val;
  }

  base::FilePath filename_;
  bool* error_;
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
