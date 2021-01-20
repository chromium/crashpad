// Copyright 2020 The Crashpad Authors. All rights reserved.
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

#include "util/ios/ios_intermediatedump_writer.h"

#include <fcntl.h>
#include <stdint.h>
#include <string.h>

namespace crashpad {
namespace internal {

bool IOSIntermediatedumpWriter::Close() {
  static uint8_t t = DOCUMENT_END;
  write(fd_, &t, sizeof(uint8_t));
  close(fd_);
  return true;
}

bool IOSIntermediatedumpWriter::Open(const base::FilePath& path) {
  fd_ = open_dprotected_np(path.value().c_str(),
                           O_WRONLY | O_CREAT | O_TRUNC,
                           // PROTECTION_CLASS_D
                           4,
                           // <empty>
                           0,
                           //-rw-r--r--
                           0644);
  return true;
}

void IOSIntermediatedumpWriter::ArrayMapStart() {
  static uint8_t t = MAP_START;
  write(fd_, &t, sizeof(uint8_t));
}

void IOSIntermediatedumpWriter::MapStart(IntermediateDumpKey key) {
  static uint8_t t = MAP_START;
  write(fd_, &t, sizeof(uint8_t));
  write(fd_, &key, sizeof(key));
}

void IOSIntermediatedumpWriter::ArrayStart(IntermediateDumpKey key) {
  static uint8_t t = ARRAY_START;
  write(fd_, &t, sizeof(uint8_t));
  write(fd_, &key, sizeof(key));
}

void IOSIntermediatedumpWriter::MapEnd() {
  static uint8_t t = MAP_END;
  write(fd_, &t, sizeof(uint8_t));
}

void IOSIntermediatedumpWriter::ArrayEnd() {
  static uint8_t t = ARRAY_END;
  write(fd_, &t, sizeof(uint8_t));
}

void IOSIntermediatedumpWriter::AddProperty(IntermediateDumpKey key,
                                            const void* value,
                                            size_t value_length) {
  mach_vm_address_t page_region_address = mach_vm_trunc_page(value);
  mach_vm_size_t page_region_size = mach_vm_round_page(
      (vm_address_t)value - page_region_address + value_length);
  vm_address_t vm_read_data;
  mach_msg_type_number_t vm_read_data_count = 0;
  kern_return_t kr = vm_read(mach_task_self(),
                             page_region_address,
                             page_region_size,
                             &vm_read_data,
                             &vm_read_data_count);
  if (kr == KERN_SUCCESS) {
    const void* vm_read_offset_data = reinterpret_cast<const void*>(
        vm_read_data + ((vm_address_t)value - page_region_address));
    uint8_t t = PROPERTY;
    write(fd_, &t, sizeof(t));
    write(fd_, &key, sizeof(key));
    //    off_t value_length_offset = lseek(fd_, 0, SEEK_CUR);
    write(fd_, &value_length, sizeof(size_t));
    ssize_t actual_length = write(fd_, vm_read_offset_data, value_length);
    if (actual_length != (ssize_t)value_length) {
      // Seek back and zero out this property.
      //      lseek(fd_, value_length_offset, SEEK_SET);
      //      constexpr size_t zero = 0;
      //      actual_length = write(fd_, &zero, sizeof(size_t));
    }
    vm_deallocate(mach_task_self(), vm_read_data, vm_read_data_count);
  }
}

size_t IOSIntermediatedumpWriter::ThreadStateLengthForFlavor(
    thread_state_flavor_t flavor) {
#if defined(ARCH_CPU_X86_64)
  switch (flavor) {
    case x86_THREAD_STATE:
      return sizeof(x86_thread_state_t);
    case x86_FLOAT_STATE:
      return sizeof(x86_float_state_t);
    case x86_DEBUG_STATE:
      return sizeof(x86_debug_state_t);
    case x86_THREAD_STATE32:
      return sizeof(x86_thread_state32_t);
    case x86_FLOAT_STATE32:
      return sizeof(x86_float_state32_t);
    case x86_DEBUG_STATE32:
      return sizeof(x86_debug_state32_t);
    default:
      return 0;
  }
#elif defined(ARCH_CPU_ARM64)
  switch (flavor) {
    case ARM_UNIFIED_THREAD_STATE:
      return sizeof(arm_unified_thread_state_t);
    case ARM_THREAD_STATE64:
      return sizeof(arm_thread_state64_t);
    case ARM_NEON_STATE64:
      return sizeof(arm_neon_state64_t);
    case ARM_DEBUG_STATE64:
      return sizeof(arm_debug_state64_t);
    default:
      return 0;
  }
#endif
}

}  // namespace internal
}  // namespace crashpad
