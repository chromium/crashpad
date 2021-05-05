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

#include <unistd.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "util/mach/mach_extensions.h"

namespace crashpad {
namespace internal {

//! \brief A scoped wrapper for calls to `vm_read` and `vm_deallocate`.  Allows
//!     in-process handler to safely read memory for the intermediate dump.
//!
//! Note: RUNS-DURING-CRASH.
template <typename T>
class ScopedVMRead {
 public:
  //! \brief vm_read value by type T.
  //!
  //! \param[in] value A t used to create a user-defined type.
  //! \param[in] value_length Length of value or sizeof(T).
  explicit ScopedVMRead(const T* value, size_t value_length = sizeof(T)) {
    reset(value, value_length);
  }

  //! \brief vm_read value by address.
  //!
  //! \param[in] value A t used to create a user-defined type.
  //! \param[in] value_length Length of value or sizeof(T).
  explicit ScopedVMRead(vm_address_t value, size_t value_length = sizeof(T)) {
    reset(reinterpret_cast<T*>(value), value_length);
  }

  ~ScopedVMRead() {
    if (data_) {
      vm_deallocate(mach_task_self(), vm_read_data_, vm_read_data_count_);
    }
  }

  //! \brief vm_read value, or log an error if unable.
  //!
  //! Note: RUNS-DURING-CRASH.
  //!
  //! \param[in] value A t used to create a user-defined type.
  //! \param[in] value_length Length of value or sizeof(T).
  void reset(const void* value, size_t value_length = sizeof(T)) {
    if (data_) {
      vm_deallocate(mach_task_self(), vm_read_data_, vm_read_data_count_);
      data_ = 0;
    }
    vm_address_t value_address = reinterpret_cast<vm_address_t>(value);
    mach_vm_address_t page_region_address = mach_vm_trunc_page(value);
    mach_vm_size_t page_region_size =
        mach_vm_round_page(value_address - page_region_address + value_length);
    kern_return_t kr = vm_read(mach_task_self(),
                               page_region_address,
                               page_region_size,
                               &vm_read_data_,
                               &vm_read_data_count_);

    if (kr == KERN_SUCCESS) {
      data_ = vm_read_data_ + (value_address - page_region_address);
    } else {
      PLOG(WARNING) << "vm_read";
    }
  }

  //! \brief Returns the pointer to memory safe to read during the in-process
  //!   crash handler.
  T* operator->() const { return get(); }

  //! \brief Returns the pointer to memory safe to read during the in-process
  //!   crash handler.
  T* get() const { return (T*)data_; }

  //! \brief Returns `true` if the vm_read was able to read the data.
  bool is_valid() const { return !!data_ && vm_read_data_count_ > 0; }

 private:
  vm_address_t data_;
  vm_address_t vm_read_data_;
  mach_msg_type_number_t vm_read_data_count_;
  DISALLOW_COPY_AND_ASSIGN(ScopedVMRead);
};

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_UTIL_IOS_SCOPED_VM_READ_H_
