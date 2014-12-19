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

#ifndef CRASHPAD_UTIL_FILE_FILE_WRITER_H_
#define CRASHPAD_UTIL_FILE_FILE_WRITER_H_

#include <fcntl.h>
#include <stddef.h>

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "build/build_config.h"
#include "util/file/file_io.h"

namespace crashpad {

//! \brief A version of `iovec` with a `const` #iov_base field.
//!
//! This structure is intended to be used for write operations.
//
// Type compatibility with iovec is tested with static assertions in the
// implementation file.
struct WritableIoVec {
  //! \brief The base address of a memory region for output.
  const void* iov_base;

  //! \brief The size of the memory pointed to by #iov_base.
  size_t iov_len;
};

//! \brief An interface to write to files and other file-like objects with
//!     semantics matching the underlying platform (POSIX or Windows).
class FileWriterInterface {
 public:
  //! \brief Wraps WriteFile(), or provides an implementation with identical
  //!     semantics.
  //!
  //! \return `true` if the operation succeeded, `false` if it failed, with an
  //!     error message logged.
  virtual bool Write(const void* data, size_t size) = 0;

  //! \brief Wraps `writev()` on POSIX or provides an alternate implementation
  //!     with identical semantics. This method will write entire buffers,
  //!     continuing after a short write or after being interrupted. On
  //!     non-POSIX this is a simple wrapper around Write().
  //!
  //! \return `true` if the operation succeeded, `false` if it failed, with an
  //!     error message logged.
  //!
  //! \note The contents of \a iovecs are undefined when this method returns.
  virtual bool WriteIoVec(std::vector<WritableIoVec>* iovecs) = 0;

  //! \brief Wraps LoggingFileSeek() or provides an alternate implementation
  //!     with identical semantics.
  //!
  //! \return The return value of LoggingFileSeek(). `-1` on failure,
  //!     with an error message logged.
  virtual FileOffset Seek(FileOffset offset, int whence) = 0;

 protected:
  ~FileWriterInterface() {}
};

//! \brief A file writer implementation that wraps traditional system file
//!     operations on files accessed through the filesystem.
class FileWriter : public FileWriterInterface {
 public:
  FileWriter();
  ~FileWriter();

  //! \brief Wraps LoggingOpenFileForWrite().
  //!
  //! \return `true` if the operation succeeded, `false` if it failed, with an
  //!     error message logged.
  //!
  //! \note After a successful call, this method cannot be called again until
  //!     after Close().
  bool Open(const base::FilePath& path,
            FileWriteMode write_mode,
            bool world_readable);

  //! \brief Wraps CheckedCloseHandle().
  //!
  //! \note It is only valid to call this method on an object that has had a
  //!     successful Open() that has not yet been matched by a subsequent call
  //!     to this method.
  void Close();

  // FileWriterInterface:

  //! \copydoc FileWriterInterface::Write()
  //!
  //! \note It is only valid to call this method between a successful Open() and
  //!     a Close().
  bool Write(const void* data, size_t size) override;

  //! \copydoc FileWriterInterface::WriteIoVec()
  //!
  //! \note It is only valid to call this method between a successful Open() and
  //!     a Close().
  bool WriteIoVec(std::vector<WritableIoVec>* iovecs) override;

  //! \copydoc FileWriterInterface::Seek()
  //!
  //! \note It is only valid to call this method between a successful Open() and
  //!     a Close().
  FileOffset Seek(FileOffset offset, int whence) override;

 private:
  ScopedFileHandle file_;

  DISALLOW_COPY_AND_ASSIGN(FileWriter);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_FILE_FILE_WRITER_H_
