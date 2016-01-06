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
#include <sys/types.h>

#include "base/logging.h"
#include "snapshot/mac/process_types/internal.h"
#include "util/mach/task_memory.h"

namespace crashpad {
namespace process_types {
namespace internal {

namespace {

template <typename T>
bool ReadIntoVersioned(ProcessReader* process_reader,
                       mach_vm_address_t address,
                       T* specific) {
  TaskMemory* task_memory = process_reader->Memory();
  if (!task_memory->Read(
          address, sizeof(specific->version), &specific->version)) {
    return false;
  }

  mach_vm_size_t size = T::ExpectedSizeForVersion(specific->version);

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

}  // namespace

// static
template <typename Traits>
size_t dyld_all_image_infos<Traits>::ExpectedSizeForVersion(
    decltype(dyld_all_image_infos<Traits>::version) version) {
  if (version >= 14) {
    return sizeof(dyld_all_image_infos<Traits>);
  }
  if (version >= 13) {
    return offsetof(dyld_all_image_infos<Traits>, reserved);
  }
  if (version >= 12) {
    return offsetof(dyld_all_image_infos<Traits>, sharedCacheUUID);
  }
  if (version >= 11) {
    return offsetof(dyld_all_image_infos<Traits>, sharedCacheSlide);
  }
  if (version >= 10) {
    return offsetof(dyld_all_image_infos<Traits>, errorKind);
  }
  if (version >= 9) {
    return offsetof(dyld_all_image_infos<Traits>, initialImageCount);
  }
  if (version >= 8) {
    return offsetof(dyld_all_image_infos<Traits>, dyldAllImageInfosAddress);
  }
  if (version >= 7) {
    return offsetof(dyld_all_image_infos<Traits>, uuidArrayCount);
  }
  if (version >= 6) {
    return offsetof(dyld_all_image_infos<Traits>, systemOrderFlag);
  }
  if (version >= 5) {
    return offsetof(dyld_all_image_infos<Traits>, coreSymbolicationShmPage);
  }
  if (version >= 3) {
    return offsetof(dyld_all_image_infos<Traits>, dyldVersion);
  }
  if (version >= 2) {
    return offsetof(dyld_all_image_infos<Traits>, jitInfo);
  }
  if (version >= 1) {
    return offsetof(dyld_all_image_infos<Traits>, libSystemInitialized);
  }
  return offsetof(dyld_all_image_infos<Traits>, infoArrayCount);
}

// static
template <typename Traits>
bool dyld_all_image_infos<Traits>::ReadInto(
    ProcessReader* process_reader,
    mach_vm_address_t address,
    dyld_all_image_infos<Traits>* specific) {
  return ReadIntoVersioned(process_reader, address, specific);
}

// static
template <typename Traits>
size_t crashreporter_annotations_t<Traits>::ExpectedSizeForVersion(
    decltype(crashreporter_annotations_t<Traits>::version) version) {
  if (version >= 5) {
    return sizeof(crashreporter_annotations_t<Traits>);
  }
  if (version >= 4) {
    return offsetof(crashreporter_annotations_t<Traits>, unknown_0);
  }
  return offsetof(crashreporter_annotations_t<Traits>, message);
}

// static
template <typename Traits>
bool crashreporter_annotations_t<Traits>::ReadInto(
    ProcessReader* process_reader,
    mach_vm_address_t address,
    crashreporter_annotations_t<Traits>* specific) {
  return ReadIntoVersioned(process_reader, address, specific);
}

// Explicit template instantiation of the above.
#define PROCESS_TYPE_FLAVOR_TRAITS(lp_bits)                                    \
  template size_t                                                              \
      dyld_all_image_infos<Traits##lp_bits>::ExpectedSizeForVersion(           \
         decltype(dyld_all_image_infos<Traits##lp_bits>::version));            \
  template bool dyld_all_image_infos<Traits##lp_bits>::ReadInto(               \
      ProcessReader*,                                                          \
      mach_vm_address_t,                                                       \
      dyld_all_image_infos<Traits##lp_bits>*);                                 \
  template size_t                                                              \
      crashreporter_annotations_t<Traits##lp_bits>::ExpectedSizeForVersion(    \
          decltype(crashreporter_annotations_t<Traits##lp_bits>::version));    \
  template bool crashreporter_annotations_t<Traits##lp_bits>::ReadInto(        \
      ProcessReader*,                                                          \
      mach_vm_address_t,                                                       \
      crashreporter_annotations_t<Traits##lp_bits>*);

#include "snapshot/mac/process_types/flavors.h"

#undef PROCESS_TYPE_FLAVOR_TRAITS

}  // namespace internal
}  // namespace process_types
}  // namespace crashpad
