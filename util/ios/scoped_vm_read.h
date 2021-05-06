// Copyright 2021 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_UTIL_IOS_SCOPED_VM_READ_H_
#define CRASHPAD_UTIL_IOS_SCOPED_VM_READ_H_

#include <mach/mach.h>

#include "base/logging.h"
#include "base/macros.h"
#include "util/mach/mach_extensions.h"

namespace crashpad {
namespace internal {

namespace vmread_internal {

//! \brief Non-templated internal class to be used by ScopedVMRead.
//!
//! Note: RUNS-DURING-CRASH.
class ScopedVMReadInternal {
 public:
  ScopedVMReadInternal();
  ~ScopedVMReadInternal();

  //! \brief Releases any previously read data and vm_reads \a data. Logs an
  //!     error on failure.
  //!
  //! \param[in] data Memory to be read by vm_read.
  //! \param[in] data_length Length of \a data.
  void reset(const void* data, size_t data_length);

  //! \brief Returns `true` if the vm_read was able to read the data.
  bool is_valid() const;

  vm_address_t data() const;

 private:
  vm_address_t data_;
  vm_address_t vm_read_data_;
  mach_msg_type_number_t vm_read_data_count_;
};

}  // namespace vmread_internal

//! \brief A scoped wrapper for calls to `vm_read` and `vm_deallocate`.  Allows
//!     in-process handler to safely read memory for the intermediate dump.
//!
//! Note: RUNS-DURING-CRASH.
template <typename T>
class ScopedVMRead {
 public:
  //! \brief vm_read data of type T.
  //!
  //! \param[in] data Memory to be read by vm_read.
  //! \param[in] data_length Length of \a data or sizeof(T).
  explicit ScopedVMRead(const T* data, size_t data_length = sizeof(T)) {
    internal.reset(data, data_length);
  }

  //! \brief vm_read \a data by address.
  //!
  //! \param[in] data Memory to be read by vm_read.
  //! \param[in] data_length Length of \a data or sizeof(T).
  explicit ScopedVMRead(vm_address_t data, size_t data_length = sizeof(T)) {
    internal.reset(reinterpret_cast<T*>(data), data_length);
  }

  //! \brief Releases any previously read data and vm_reads data. Logs an error
  //!     on failure.
  //!
  //! \param[in] data Memory to be read by vm_read.
  //! \param[in] data_length Length of \a data or sizeof(T).
  void reset(const void* data, size_t data_length = sizeof(T)) {
    internal.reset(data, data_length);
  }

  //! \brief Returns the pointer to memory safe to read during the in-process
  //!   crash handler.
  T* operator->() const { return get(); }

  //! \brief Returns the pointer to memory safe to read during the in-process
  //!   crash handler.
  T* get() const { return reinterpret_cast<T*>(internal.data()); }

  //! \brief Returns `true` if the vm_read was able to read the data.
  bool is_valid() const { return internal.is_valid(); }

 private:
  vmread_internal::ScopedVMReadInternal internal;
  DISALLOW_COPY_AND_ASSIGN(ScopedVMRead);
};

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_UTIL_IOS_SCOPED_VM_READ_H_
