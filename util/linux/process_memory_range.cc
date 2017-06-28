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

#include "util/linux/process_memory_range.h"

#include <algorithm>

namespace crashpad {

ProcessMemoryRange::~ProcessMemoryRange() {}

bool ProcessMemoryRange::Initialize(const ProcessMemory* memory,
                                    bool is_64_bit,
                                    LinuxVMAddress base,
                                    LinuxVMSize size) {
  memory_ = memory;
  range_.SetRange(is_64_bit, base, size);
  return range_.IsValid();
}

bool ProcessMemoryRange::RestrictRange(LinuxVMAddress base,
                                       LinuxVMSize size) {
  if (base < range_.Base() || base + size > range_.End()) {
    LOG(ERROR) << "new range outside old range";
    return false;
  }
  range_.SetRange(range_.Is64Bit(), base, size);
  return true;
}

bool ProcessMemoryRange::Read(LinuxVMAddress address,
                              size_t size,
                              void* buffer) const {
  if (!range_.ContainsRange(CheckedLinuxAddressRange(
      range_.Is64Bit(), address, size))) {
    LOG(ERROR) << "read out of range";
    return false;
  }
  return memory_->Read(address, size, buffer);
}

bool ProcessMemoryRange::ReadCStringSizeLimited(LinuxVMAddress address,
                                                size_t size,
                                                std::string* string) const {
  if (!range_.ContainsValue(address)) {
    LOG(ERROR) << "read out of range";
    return false;
  }
  size = std::min(static_cast<LinuxVMSize>(size), range_.End() - address);
  return memory_->Read(address, size, string);
}

}  // namespace crashpad
