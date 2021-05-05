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

//! \brief FILLMEIN
template <typename T>
class ScopedVMRead {
 public:
  //! \brief FILLMEIN
  explicit ScopedVMRead(const T* value, size_t value_length = sizeof(T)) {
    reset(value, value_length);
  }

  //! \brief FILLMEIN
  explicit ScopedVMRead(uint64_t value, size_t value_length = sizeof(T)) {
    reset(reinterpret_cast<T*>(value), value_length);
  }

  ~ScopedVMRead() {
    if (data_) {
      release();
    }
  }

  //! \brief FILLMEIN
  void reset(const void* value, size_t value_length = sizeof(T)) {
    if (data_) {
      release();
    }
    mach_vm_address_t page_region_address = mach_vm_trunc_page(value);
    mach_vm_size_t page_region_size = mach_vm_round_page(
        (vm_address_t)value - page_region_address + value_length);
    kern_return_t kr = vm_read(mach_task_self(),
                               page_region_address,
                               page_region_size,
                               &vm_read_data_,
                               &vm_read_data_count_);
    if (kr == KERN_SUCCESS) {
      data_ = vm_read_data_ + ((vm_address_t)value - page_region_address);
    } else {
      PLOG(WARNING) << "vm_read";
    }
  }

  //! \brief FILLMEIN
  T* operator->() const { return get(); }
  T* get() const { return (T*)data_; }

  //! \brief FILLMEIN
  bool is_valid() const { return !!data_ && vm_read_data_count_ > 0; }

 private:
  void release() {
    vm_deallocate(mach_task_self(), vm_read_data_, vm_read_data_count_);
    data_ = 0;
  }
  vm_address_t data_ = 0;
  vm_address_t vm_read_data_;
  mach_msg_type_number_t vm_read_data_count_ = 0;
  DISALLOW_COPY_AND_ASSIGN(ScopedVMRead);
};

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_UTIL_IOS_SCOPED_VM_READ_H_
