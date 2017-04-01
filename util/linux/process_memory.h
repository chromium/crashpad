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

#ifndef CRASHPAD_UTIL_LINUX_PROCESS_MEMORY_H_
#define CRASHPAD_UTIL_LINUX_PROCESS_MEMORY_H_

#include <sys/types.h>

#include <string>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "util/linux/address_types.h"

namespace crashpad {

//! \brief Accesses the memory of another process.
class ProcessMemory {
 public:
  ProcessMemory();
  ~ProcessMemory();

  //! \brief Initializes this object to read the memory of a process whose ID
  //!     is \a pid.
  //!
  //! This method must be called successfully prior to calling any other method
  //! in this class.
  //!
  //! \param[in] pid The process ID of a target process.
  //!
  //! \return `true` on success, `false` on failure with a message logged.
  bool Initialize(pid_t pid);

  //! \brief Copies memory from the target process into a caller-provided buffer
  //!     in the current process.
  //!
  //! \param[in] address The address, in the target process' address space, of
  //!     the memory region to copy.
  //! \param[in] size The size, in bytes, of the memory region to copy.
  //!     \a buffer must be at least this size.
  //! \param[out] buffer The buffer into which the contents of the other
  //!     process' memory will be copied.
  //!
  //! \return `true` on success, with \a buffer filled appropriately. `false` on
  //!     failure, with a message logged.
  bool Read(LinuxVMAddress address, size_t size, void* buffer) const;

  //! \brief Reads a `NUL`-terminated C string from the target process into a
  //!     string in the current process.
  //!
  //! The length of the string need not be known ahead of time. This method will
  //! read contiguous memory until a `NUL` terminator is found.
  //!
  //! \param[in] address The address, in the target process’s address space, of
  //!     the string to copy.
  //! \param[out] string The string read from the other process.
  //!
  //! \return `true` on success, with \a string set appropriately. `false` on
  //!     failure, with a message logged. Failures can occur, for example, when
  //!     encountering unmapped or unreadable pages.
  bool ReadCString(LinuxVMAddress address, std::string* string) const;

  //! \brief Reads a `NUL`-terminated C string from the target process into a
  //!     string in the current process.
  //!
  //! \param[in] address The address, in the target process’s address space, of
  //!     the string to copy.
  //! \param[in] size The maximum number of bytes to read. The string is
  //!     required to be `NUL`-terminated within this many bytes.
  //! \param[out] string The string read from the other process.
  //!
  //! \return `true` on success, with \a string set appropriately. `false` on
  //!     failure, with a message logged. Failures can occur, for example, when
  //!     a `NUL` terminator is not found within \a size bytes, or when
  //!     encountering unmapped or unreadable pages.
  bool ReadCStringSizeLimited(LinuxVMAddress address,
                              size_t size,
                              std::string* string) const;

 private:
  bool ReadCStringInternal(LinuxVMAddress address,
                           bool has_size,
                           size_t size,
                           std::string* string) const;

  base::ScopedFD mem_fd_;
  pid_t pid_;

  DISALLOW_COPY_AND_ASSIGN(ProcessMemory);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_LINUX_PROCESS_MEMORY_H_
