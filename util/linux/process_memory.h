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

#include "base/macros.h"
#include "util/linux/address_types.h"

namespace crashpad {

//! \brief Accesses the memory of another process.
class ProcessMemory {
 public:
  //! \param[in] pid The pid of a target process.
  explicit ProcessMemory(pid_t pid);
  ~ProcessMemory();

  //! \brief Copies memory from the target process into a caller-provided buffer
  // in the current process.
  //
  // \param[in] address The address, in the target process's address space, of
  // the memory region to copy.
  // \param[in] size The size, in bytes, of the memory region to copy. \a buffer
  // must be at least this size.
  // \param[out] buffer The buffer into which the contents of the other
  // process's memory will be copied.
  //
  // \return `true` on success, with \a buffer filled appropriately. `false` on
  // failure, with a warning logged.
  bool Read(LinuxVMAddress address, LinuxVMSize size, void* buffer) const;

  //! \brief Reads a `NUL`-terminated C string from the target process into a
  //!     string in the current process.
  //!
  //! The length of the string need not be known ahead of time. This method will
  //! read contiguous memory until a `NUL` terminator is found.
  //!
  //! \param[in] address The address, in the target task’s address space, of the
  //!     string to copy.
  //! \param[out] string The string read from the other task.
  //!
  //! \return `true` on success, with \a string set appropriately. `false` on
  //!     failure, with a warning logged. Failures can occur, for example, when
  //!     encountering unmapped or unreadable pages.
  //!
  bool ReadCString(LinuxVMAddress address, std::string* string) const;

  //! \brief Reads a `NUL`-terminated C string from the target task into a
  //!     string in the current task.
  //!
  //! \param[in] address The address, in the target task’s address space, of the
  //!     string to copy.
  //! \param[in] size The maximum number of bytes to read. The string is
  //!     required to be `NUL`-terminated within this many bytes.
  //! \param[out] string The string read from the other task.
  //!
  //! \return `true` on success, with \a string set appropriately. `false` on
  //!     failure, with a warning logged. Failures can occur, for example, when
  //!     a `NUL` terminator is not found within \a size bytes, or when
  //!     encountering unmapped or unreadable pages.
  //!
  //! \sa MappedMemory::ReadCString()
  bool ReadCStringSizeLimited(LinuxVMAddress address,
                              LinuxVMSize size,
                              std::string* string) const;

 private:
  pid_t pid_;

  bool ReadCStringInternal(LinuxVMAddress address,
                           bool has_size,
                           LinuxVMSize size,
                           std::string* string) const;
  bool PtraceRead(LinuxVMAddress address, LinuxVMSize size, void* buffer) const;
  bool ProcFSRead(LinuxVMAddress address, LinuxVMSize size, void* buffer) const;

  DISALLOW_COPY_AND_ASSIGN(ProcessMemory);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_LINUX_PROCESS_MEMORY_H_
