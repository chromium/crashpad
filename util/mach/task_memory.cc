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

#include "util/mach/task_memory.h"

#include <mach/mach_vm.h>
#include <string.h>

#include <algorithm>

#include "base/logging.h"
#include "base/mac/mach_logging.h"
#include "base/mac/scoped_mach_vm.h"
#include "base/strings/stringprintf.h"

namespace crashpad {

TaskMemory::TaskMemory(mach_port_t task) : task_(task) {
}

bool TaskMemory::Read(mach_vm_address_t address, size_t size, void* buffer) {
  if (size == 0) {
    return true;
  }

  mach_vm_address_t region_address = mach_vm_trunc_page(address);
  mach_vm_size_t region_size =
      mach_vm_round_page(address - region_address + size);

  vm_offset_t region;
  mach_msg_type_number_t region_count;
  kern_return_t kr =
      mach_vm_read(task_, region_address, region_size, &region, &region_count);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(WARNING, kr) << base::StringPrintf(
        "mach_vm_read(0x%llx, 0x%llx)", region_address, region_size);
    return false;
  }

  DCHECK_EQ(region_count, region_size);
  base::mac::ScopedMachVM region_owner(region, region_count);

  const char* region_base = reinterpret_cast<const char*>(region);
  memcpy(buffer, &region_base[address - region_address], size);

  return true;
}

bool TaskMemory::ReadCString(mach_vm_address_t address, std::string* string) {
  return ReadCStringInternal(address, false, 0, string);
}

bool TaskMemory::ReadCStringSizeLimited(mach_vm_address_t address,
                                        mach_vm_size_t size,
                                        std::string* string) {
  return ReadCStringInternal(address, true, size, string);
}

bool TaskMemory::ReadCStringInternal(mach_vm_address_t address,
                                     bool has_size,
                                     mach_vm_size_t size,
                                     std::string* string) {
  if (has_size) {
    if (size == 0)  {
      string->clear();
      return true;
    }
  } else {
    size = PAGE_SIZE;
  }

  std::string local_string;
  mach_vm_address_t read_address = address;
  do {
    mach_vm_size_t read_length =
        std::min(size, PAGE_SIZE - (read_address % PAGE_SIZE));
    std::string read_string(read_length, '\0');
    if (!Read(read_address, read_length, &read_string[0])) {
      return false;
    }

    size_t terminator = read_string.find_first_of('\0');
    if (terminator == std::string::npos) {
      local_string.append(read_string);
    } else {
      local_string.append(read_string, 0, terminator);
      string->swap(local_string);
      return true;
    }

    if (has_size) {
      size -= read_length;
    }
    read_address = mach_vm_trunc_page(read_address + read_length);
  } while ((!has_size || size > 0) && read_address > address);

  LOG(WARNING) << base::StringPrintf("unterminated string at 0x%llx", address);
  return false;
}

}  // namespace crashpad
