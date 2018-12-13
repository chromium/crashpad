// Copyright 2018 The Crashpad Authors. All rights reserved.
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

#include "snapshot/fuchsia/memory_map_region_snapshot_fuchsia.h"

#include "base/logging.h"

namespace crashpad {
namespace internal {

namespace {

// Maps from bitwise OR of Zircon's flags to enumerated Windows version.
uint32_t MmuFlagsToProtectFlags(zx_vm_option_t flags) {
  const struct {
    zx_vm_option_t from;
    uint32_t to;
  } mappings[] = {
      {0, PAGE_NOACCESS},  // ---
      {ZX_VM_PERM_WRITE, PAGE_WRITECOPY},  // -w-
      {ZX_VM_PERM_EXECUTE, PAGE_EXECUTE},  // --x
      {ZX_VM_PERM_WRITE | ZX_VM_PERM_EXECUTE, PAGE_EXECUTE_WRITECOPY},  // -wx
      {ZX_VM_PERM_READ, PAGE_READONLY},  // r--
      {ZX_VM_PERM_READ | ZX_VM_PERM_WRITE, PAGE_READWRITE},  // rw-
      {ZX_VM_PERM_READ | ZX_VM_PERM_EXECUTE, PAGE_EXECUTE_READ},  // r-x
      {ZX_VM_PERM_READ | ZX_VM_PERM_WRITE | ZX_VM_PERM_EXECUTE,
       PAGE_EXECUTE_READWRITE},  // rwx
  };

  flags &= ZX_VM_PERM_READ | ZX_VM_PERM_WRITE | ZX_VM_PERM_EXECUTE;

  for (const auto& mapping : mappings) {
    if (mapping.from == flags) {
      return mapping.to;
    }
  }

  // The above mapping table should be exhaustive for the lower 3 bits.
  NOTREACHED();
  return 0;
}

}  // namespace

MemoryMapRegionSnapshotFuchsia::MemoryMapRegionSnapshotFuchsia(
    const zx_info_maps_t& info_map)
    : memory_info_() {
  DCHECK_EQ(info_map.type, ZX_INFO_MAPS_TYPE_MAPPING);

  memory_info_.BaseAddress = info_map.base;
  memory_info_.AllocationBase = info_map.base;
  memory_info_.RegionSize = info_map.size;
  memory_info_.State = MEM_COMMIT;
  memory_info_.Protect = memory_info_.AllocationProtect =
      MmuFlagsToProtectFlags(info_map.u.mapping.mmu_flags);
  memory_info_.Type = MEM_MAPPED;
}

MemoryMapRegionSnapshotFuchsia::~MemoryMapRegionSnapshotFuchsia() {}

const MINIDUMP_MEMORY_INFO&
MemoryMapRegionSnapshotFuchsia::AsMinidumpMemoryInfo() const {
  return memory_info_;
}

}  // namespace internal
}  // namespace crashpad
