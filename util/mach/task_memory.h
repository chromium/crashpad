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

#ifndef CRASHPAD_UTIL_MACH_TASK_MEMORY_H_
#define CRASHPAD_UTIL_MACH_TASK_MEMORY_H_

#include <mach/mach.h>

#include <string>

#include "base/basictypes.h"

namespace crashpad {

//! \brief Accesses the memory of another Mach task.
class TaskMemory {
 public:
  //! \param[in] task A send right to the target task’s task port. This object
  //!     does not take ownership of the send right.
  explicit TaskMemory(mach_port_t task);

  ~TaskMemory() {}

  //! \brief Copies memory from the target task into a user-provided buffer in
  //!     the current task.
  //!
  //! \param[in] address The address, in the target task’s address space, of the
  //!     memory region to copy.
  //! \param[in] size The size, in bytes, of the memory region to copy. \a
  //!     buffer must be at least this size.
  //! \param[out] buffer The buffer into which the contents of the other task’s
  //!     memory will be copied.
  //!
  //! \return `true` on success, with \a buffer filled appropriately. `false` on
  //!     failure, with a warning logged. Failures can occur, for example, when
  //!     encountering unmapped or unreadable pages.
  bool Read(mach_vm_address_t address, size_t size, void* buffer);

  //! \brief Reads a `NUL`-terminated C string from the target task into a
  //!     string in the current task.
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
  bool ReadCString(mach_vm_address_t address, std::string* string);

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
  bool ReadCStringSizeLimited(mach_vm_address_t address,
                              mach_vm_size_t size,
                              std::string* string);

 private:
  // The common internal implementation shared by the ReadCString*() methods.
  bool ReadCStringInternal(mach_vm_address_t address,
                           bool has_size,
                           mach_vm_size_t size,
                           std::string* string);

  mach_port_t task_;  // weak

  DISALLOW_COPY_AND_ASSIGN(TaskMemory);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_MACH_TASK_MEMORY_H_
