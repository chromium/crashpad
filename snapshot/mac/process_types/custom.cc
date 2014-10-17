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

#include "snapshot/mac/process_types.h"

#include <string.h>

#include "snapshot/mac/process_types/internal.h"
#include "util/mach/task_memory.h"

namespace crashpad {
namespace process_types {
namespace internal {

template <typename Traits>
bool dyld_all_image_infos<Traits>::ReadInto(
    ProcessReader* process_reader,
    mach_vm_address_t address,
    dyld_all_image_infos<Traits>* specific) {
  TaskMemory* task_memory = process_reader->Memory();
  if (!task_memory->Read(
          address, sizeof(specific->version), &specific->version)) {
    return false;
  }

  mach_vm_size_t size;
  if (specific->version >= 14) {
    size = sizeof(dyld_all_image_infos<Traits>);
  } else if (specific->version >= 13) {
    size = offsetof(dyld_all_image_infos<Traits>, reserved);
  } else if (specific->version >= 12) {
    size = offsetof(dyld_all_image_infos<Traits>, sharedCacheUUID);
  } else if (specific->version >= 11) {
    size = offsetof(dyld_all_image_infos<Traits>, sharedCacheSlide);
  } else if (specific->version >= 10) {
    size = offsetof(dyld_all_image_infos<Traits>, errorKind);
  } else if (specific->version >= 9) {
    size = offsetof(dyld_all_image_infos<Traits>, initialImageCount);
  } else if (specific->version >= 8) {
    size = offsetof(dyld_all_image_infos<Traits>, dyldAllImageInfosAddress);
  } else if (specific->version >= 7) {
    size = offsetof(dyld_all_image_infos<Traits>, uuidArrayCount);
  } else if (specific->version >= 6) {
    size = offsetof(dyld_all_image_infos<Traits>, systemOrderFlag);
  } else if (specific->version >= 5) {
    size = offsetof(dyld_all_image_infos<Traits>, coreSymbolicationShmPage);
  } else if (specific->version >= 3) {
    size = offsetof(dyld_all_image_infos<Traits>, dyldVersion);
  } else if (specific->version >= 2) {
    size = offsetof(dyld_all_image_infos<Traits>, jitInfo);
  } else if (specific->version >= 1) {
    size = offsetof(dyld_all_image_infos<Traits>, libSystemInitialized);
  } else {
    size = offsetof(dyld_all_image_infos<Traits>, infoArrayCount);
  }

  if (!task_memory->Read(address, size, specific)) {
    return false;
  }

  // Zero out the rest of the structure in case anything accesses fields without
  // checking the version.
  size_t remaining = sizeof(*specific) - size;
  if (remaining > 0) {
    char* start = reinterpret_cast<char*>(specific) + size;
    memset(start, 0, remaining);
  }

  return true;
}

#define PROCESS_TYPE_FLAVOR_TRAITS(lp_bits)                      \
  template bool dyld_all_image_infos<Traits##lp_bits>::ReadInto( \
      ProcessReader*,                                            \
      mach_vm_address_t,                                         \
      dyld_all_image_infos<Traits##lp_bits>*);

#include "snapshot/mac/process_types/flavors.h"

#undef PROCESS_TYPE_FLAVOR_TRAITS

}  // namespace internal
}  // namespace process_types
}  // namespace crashpad
