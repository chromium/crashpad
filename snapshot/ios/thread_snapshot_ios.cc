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

#include "snapshot/ios/thread_snapshot_ios.h"

#include <mach/vm_map.h>

#include "base/logging.h"
#include "base/mac/mach_logging.h"

namespace {

kern_return_t MachVMRegionRecurseDeepest(task_t task,
                                         vm_address_t* address,
                                         vm_size_t* size,
                                         natural_t* depth,
                                         vm_prot_t* protection,
                                         unsigned int* user_tag) {
  vm_region_submap_short_info_64 submap_info;
  mach_msg_type_number_t count = VM_REGION_SUBMAP_SHORT_INFO_COUNT_64;
  while (true) {
    kern_return_t kr = vm_region_recurse_64(
        task,
        address,
        size,
        depth,
        reinterpret_cast<vm_region_recurse_info_t>(&submap_info),
        &count);
    if (kr != KERN_SUCCESS) {
      return kr;
    }

    if (!submap_info.is_submap) {
      *protection = submap_info.protection;
      *user_tag = submap_info.user_tag;
      return KERN_SUCCESS;
    }

    ++*depth;
  }
}

}  // namespace

namespace crashpad {
namespace internal {

ThreadSnapshotIOS::ThreadSnapshotIOS()
    : ThreadSnapshot(),
      context_(),
      stack_(),
      thread_id_(0),
      thread_specific_data_address_(0),
      suspend_count_(0),
      priority_(0),
      initialized_() {}

ThreadSnapshotIOS::~ThreadSnapshotIOS() {}

// static
thread_act_array_t ThreadSnapshotIOS::AllThread(mach_msg_type_number_t* count) {
  thread_act_array_t threads;
  *count = 0;
  kern_return_t kr = task_threads(mach_task_self(), &threads, count);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(WARNING, kr) << "task_threads";
  }

  return threads;
}

bool ThreadSnapshotIOS::Initialize(thread_t thread) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  // wut?
  INITIALIZATION_STATE_SET_VALID(initialized_);

#if defined(ARCH_CPU_ARM64)
  arm_thread_state64_t thread_state;
  const thread_state_flavor_t thread_state_flavor = ARM_THREAD_STATE64;
  mach_msg_type_number_t thread_state_count = ARM_THREAD_STATE64_COUNT;

  arm_neon_state64_t float_state;
  const thread_state_flavor_t float_state_flavor = ARM_NEON_STATE64;
  mach_msg_type_number_t float_state_count = ARM_NEON_STATE64_COUNT;

  arm_debug_state64_t debug_state;
  const thread_state_flavor_t debug_state_flavor = ARM_DEBUG_STATE64;
  mach_msg_type_number_t debug_state_count = ARM_DEBUG_STATE64_COUNT;

  arm_amx_state_v1_t amx_state;
  const thread_state_flavor_t amx_state_flavor = ARM_AMX_STATE_V1;
  mach_msg_type_number_t amx_state_count = ARM_AMX_STATE_V1_COUNT;

  arm_vfp_state_t vfp_state;
  const thread_state_flavor_t vfp_state_flavor = ARM_VFP_STATE;
  mach_msg_type_number_t vfp_state_count = ARM_VFP_STATE_COUNT;
#elif defined(ARCH_CPU_X86_FAMILY)
  x86_thread_state64_t thread_state;
  const thread_state_flavor_t thread_state_flavor = x86_THREAD_STATE64;
  mach_msg_type_number_t thread_state_count = x86_THREAD_STATE64_COUNT;

  x86_float_state64_t float_state;
  const thread_state_flavor_t float_state_flavor = x86_FLOAT_STATE64;
  mach_msg_type_number_t float_state_count = x86_FLOAT_STATE64_COUNT;

  x86_debug_state64_t debug_state;
  const thread_state_flavor_t debug_state_flavor = x86_DEBUG_STATE64;
  mach_msg_type_number_t debug_state_count = x86_DEBUG_STATE64_COUNT;
#endif

  kern_return_t kr =
      thread_get_state(thread,
                       thread_state_flavor,
                       reinterpret_cast<thread_state_t>(&thread_state),
                       &thread_state_count);

  kr = thread_get_state(thread,
                        float_state_flavor,
                        reinterpret_cast<thread_state_t>(&float_state),
                        &float_state_count);

  kr = thread_get_state(thread,
                        debug_state_flavor,
                        reinterpret_cast<thread_state_t>(&debug_state),
                        &debug_state_count);

#if defined(ARCH_CPU_ARM64)
  kr = thread_get_state(thread,
                        amx_state_flavor,
                        reinterpret_cast<thread_state_t>(&amx_state),
                        &amx_state_count);

  kr = thread_get_state(thread,
                        vfp_state_flavor,
                        reinterpret_cast<thread_state_t>(&vfp_state),
                        &vfp_state_count);
#endif

  thread_basic_info basic_info;
  mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
  kr = thread_info(thread,
                   THREAD_BASIC_INFO,
                   reinterpret_cast<thread_info_t>(&basic_info),
                   &count);

  thread_identifier_info identifier_info;
  count = THREAD_IDENTIFIER_INFO_COUNT;
  kr = thread_info(thread,
                   THREAD_IDENTIFIER_INFO,
                   reinterpret_cast<thread_info_t>(&identifier_info),
                   &count);

  thread_precedence_policy precedence;
  count = THREAD_PRECEDENCE_POLICY_COUNT;
  boolean_t get_default = FALSE;
  kr = thread_policy_get(thread,
                         THREAD_PRECEDENCE_POLICY,
                         reinterpret_cast<thread_policy_t>(&precedence),
                         &count,
                         &get_default);

  vm_size_t stack_region_size;
  vm_address_t base =
      CalculateStackRegion(thread_state.__rsp, &stack_region_size);
  base = 0;

  return true;
}

vm_address_t ThreadSnapshotIOS::CalculateStackRegion(
    vm_address_t stack_pointer,
    vm_size_t* stack_region_size) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  // For pthreads, it may be possible to compute the stack region based on the
  // internal _pthread::stackaddr and _pthread::stacksize. The _pthread struct
  // for a thread can be located at TSD slot 0, or the known offsets of
  // stackaddr and stacksize from the TSD area could be used.
  vm_address_t region_base = stack_pointer;
  vm_size_t region_size;
  natural_t depth = 0;
  vm_prot_t protection;
  unsigned int user_tag;
  kern_return_t kr = MachVMRegionRecurseDeepest(mach_task_self(),
                                                &region_base,
                                                &region_size,
                                                &depth,
                                                &protection,
                                                &user_tag);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(INFO, kr) << "mach_vm_region_recurse";
    *stack_region_size = 0;
    return 0;
  }

  if (region_base > stack_pointer) {
    // There’s nothing mapped at the stack pointer’s address. Something may have
    // trashed the stack pointer. Note that this shouldn’t happen for a normal
    // stack guard region violation because the guard region is mapped but has
    // VM_PROT_NONE protection.
    *stack_region_size = 0;
    return 0;
  }

  vm_address_t start_address = stack_pointer;

  if ((protection & VM_PROT_READ) == 0) {
    // If the region isn’t readable, the stack pointer probably points to the
    // guard region. Don’t include it as part of the stack, and don’t include
    // anything at any lower memory address. The code below may still possibly
    // find the real stack region at a memory address higher than this region.
    start_address = region_base + region_size;
  } else {
    // If the ABI requires a red zone, adjust the region to include it if
    // possible.
    LocateRedZone(&start_address, &region_base, &region_size, user_tag);

    // Regardless of whether the ABI requires a red zone, capture up to
    // kExtraCaptureSize additional bytes of stack, but only if present in the
    // region that was already found.
    constexpr vm_size_t kExtraCaptureSize = 128;
    start_address = std::max(start_address >= kExtraCaptureSize
                                 ? start_address - kExtraCaptureSize
                                 : start_address,
                             region_base);

    // Align start_address to a 16-byte boundary, which can help readers by
    // ensuring that data is aligned properly. This could page-align instead,
    // but that might be wasteful.
    constexpr vm_size_t kDesiredAlignment = 16;
    start_address &= ~(kDesiredAlignment - 1);
    DCHECK_GE(start_address, region_base);
  }

  region_size -= (start_address - region_base);
  region_base = start_address;

  vm_size_t total_region_size = region_size;

  // The stack region may have gotten split up into multiple abutting regions.
  // Try to coalesce them. This frequently happens for the main thread’s stack
  // when setrlimit(RLIMIT_STACK, …) is called. It may also happen if a region
  // is split up due to an mprotect() or vm_protect() call.
  //
  // Stack regions created by the kernel and the pthreads library will be marked
  // with the VM_MEMORY_STACK user tag. Scanning for multiple adjacent regions
  // with the same tag should find an entire stack region. Checking that the
  // protection on individual regions is not VM_PROT_NONE should guarantee that
  // this algorithm doesn’t collect map entries belonging to another thread’s
  // stack: well-behaved stacks (such as those created by the kernel and the
  // pthreads library) have VM_PROT_NONE guard regions at their low-address
  // ends.
  //
  // Other stack regions may not be so well-behaved and thus if user_tag is not
  // VM_MEMORY_STACK, the single region that was found is used as-is without
  // trying to merge it with other adjacent regions.
  if (user_tag == VM_MEMORY_STACK) {
    vm_address_t try_address = region_base;
    vm_address_t original_try_address;

    while (try_address += region_size,
           original_try_address = try_address,
           (kr = MachVMRegionRecurseDeepest(mach_task_self(),
                                            &try_address,
                                            &region_size,
                                            &depth,
                                            &protection,
                                            &user_tag) == KERN_SUCCESS) &&
               try_address == original_try_address &&
               (protection & VM_PROT_READ) != 0 &&
               user_tag == VM_MEMORY_STACK) {
      total_region_size += region_size;
    }

    if (kr != KERN_SUCCESS && kr != KERN_INVALID_ADDRESS) {
      // Tolerate KERN_INVALID_ADDRESS because it will be returned when there
      // are no more regions in the map at or above the specified |try_address|.
      MACH_LOG(INFO, kr) << "vm_region_recurse";
    }
  }

  *stack_region_size = total_region_size;
  return region_base;
}

void ThreadSnapshotIOS::LocateRedZone(vm_address_t* const start_address,
                                      vm_address_t* const region_base,
                                      vm_address_t* const region_size,
                                      const unsigned int user_tag) {
  // x86_64 has a red zone. See AMD64 ABI 0.99.8,
  // https://raw.githubusercontent.com/wiki/hjl-tools/x86-psABI/x86-64-psABI-r252.pdf#page=19,
  // section 3.2.2, “The Stack Frame”.
  constexpr vm_size_t kRedZoneSize = 128;
  vm_address_t red_zone_base =
      *start_address >= kRedZoneSize ? *start_address - kRedZoneSize : 0;
  bool red_zone_ok = false;
  if (red_zone_base >= *region_base) {
    // The red zone is within the region already discovered.
    red_zone_ok = true;
  } else if (red_zone_base < *region_base && user_tag == VM_MEMORY_STACK) {
    // Probe to see if there’s a region immediately below the one already
    // discovered.
    vm_address_t red_zone_region_base = red_zone_base;
    vm_size_t red_zone_region_size;
    natural_t red_zone_depth = 0;
    vm_prot_t red_zone_protection;
    unsigned int red_zone_user_tag;
    kern_return_t kr = MachVMRegionRecurseDeepest(mach_task_self(),
                                                  &red_zone_region_base,
                                                  &red_zone_region_size,
                                                  &red_zone_depth,
                                                  &red_zone_protection,
                                                  &red_zone_user_tag);
    if (kr != KERN_SUCCESS) {
      MACH_LOG(INFO, kr) << "vm_region_recurse";
      *start_address = *region_base;
    } else if (red_zone_region_base + red_zone_region_size == *region_base &&
               (red_zone_protection & VM_PROT_READ) != 0 &&
               red_zone_user_tag == user_tag) {
      // The region containing the red zone is immediately below the region
      // already found, it’s readable (not the guard region), and it has the
      // same user tag as the region already found, so merge them.
      red_zone_ok = true;
      *region_base -= red_zone_region_size;
      *region_size += red_zone_region_size;
    }
  }

  if (red_zone_ok) {
    // Begin capturing from the base of the red zone (but not the entire
    // region that encompasses the red zone).
    *start_address = red_zone_base;
  } else {
    // The red zone would go lower into another region in memory, but no
    // region was found. Memory can only be captured to an address as low as
    // the base address of the region already found.
    *start_address = *region_base;
  }
}

const CPUContext* ThreadSnapshotIOS::Context() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &context_;
}

const MemorySnapshot* ThreadSnapshotIOS::Stack() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &stack_;
}

uint64_t ThreadSnapshotIOS::ThreadID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_id_;
}

int ThreadSnapshotIOS::SuspendCount() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return suspend_count_;
}

int ThreadSnapshotIOS::Priority() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return priority_;
}

uint64_t ThreadSnapshotIOS::ThreadSpecificDataAddress() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_specific_data_address_;
}

std::vector<const MemorySnapshot*> ThreadSnapshotIOS::ExtraMemory() const {
  return std::vector<const MemorySnapshot*>();
}

}  // namespace internal
}  // namespace crashpad
