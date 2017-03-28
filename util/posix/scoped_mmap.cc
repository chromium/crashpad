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

#include "util/posix/scoped_mmap.h"

#include "base/logging.h"

namespace crashpad {

ScopedMmap::ScopedMmap() {}

ScopedMmap::~ScopedMmap() {
  Reset();
}

void ScopedMmap::Reset() {
  ResetAddrLen(MAP_FAILED, 0);
}

void ScopedMmap::ResetAddrLen(void* addr, size_t len) {
  if (is_valid() && munmap(addr_, len_) != 0) {
    LOG(ERROR) << "munmap";
  }

  addr_ = addr;
  len_ = len;
}

bool ScopedMmap::ResetMmap(void* addr,
                           size_t len,
                           int prot,
                           int flags,
                           int fd,
                           off_t offset) {
  Reset();

  void* new_addr = mmap(addr, len, prot, flags, fd, offset);
  if (new_addr == MAP_FAILED) {
    LOG(ERROR) << "mmap";
    return false;
  }

  ResetAddrLen(new_addr, len);
  return true;
}

bool ScopedMmap::Mprotect(int prot) {
  if (mprotect(addr_, len_, prot) < 0) {
    LOG(ERROR) << "mprotect";
    return false;
  }

  return true;
}

}  // namespace crashpad
