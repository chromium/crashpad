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

#include "client/ios_handler/in_process_intermediatedump_handler.h"

#include <errno.h>
#include <fcntl.h>
#include <mach-o/dyld_images.h>
#include <mach-o/nlist.h>
#include <mach/mach.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <uuid/uuid.h>

#include "base/logging.h"
#include "build/build_config.h"
#include "client/crashpad_info.h"
#include "snapshot/snapshot_constants.h"
#include "util/ios/ios_intermediatedump_writer.h"
#include "util/misc/from_pointer_cast.h"

namespace {

#if defined(ARCH_CPU_X86_64)
const thread_state_flavor_t kThreadStateFlavor = x86_THREAD_STATE64;
const thread_state_flavor_t kFloatStateFlavor = x86_FLOAT_STATE64;
const thread_state_flavor_t kDebugStateFlavor = x86_DEBUG_STATE64;
#elif defined(ARCH_CPU_ARM64)
const thread_state_flavor_t kThreadStateFlavor = ARM_THREAD_STATE64;
const thread_state_flavor_t kFloatStateFlavor = ARM_NEON_STATE64;
#endif

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

//! \brief Adjusts the region for the red zone, if the ABI requires one.
//!
//! This method performs red zone calculation for CalculateStackRegion(). Its
//! parameters are local variables used within that method, and may be
//! modified as needed.
//!
//! Where a red zone is required, the region of memory captured for a thread’s
//! stack will be extended to include the red zone below the stack pointer,
//! provided that such memory is mapped, readable, and has the correct user
//! tag value. If these conditions cannot be met fully, as much of the red
//! zone will be captured as is possible while meeting these conditions.
//!
//! \param[in,out] start_address The base address of the region to begin
//!     capturing stack memory from. On entry, \a start_address is the stack
//!     pointer. On return, \a start_address may be decreased to encompass a
//!     red zone.
//! \param[in,out] region_base The base address of the region that contains
//!     stack memory. This is distinct from \a start_address in that \a
//!     region_base will be page-aligned. On entry, \a region_base is the
//!     base address of a region that contains \a start_address. On return,
//!     if \a start_address is decremented and is outside of the region
//!     originally described by \a region_base, \a region_base will also be
//!     decremented appropriately.
//! \param[in,out] region_size The size of the region that contains stack
//!     memory. This region begins at \a region_base. On return, if \a
//!     region_base is decremented, \a region_size will be incremented
//!     appropriately.
//! \param[in] user_tag The Mach VM system’s user tag for the region described
//!     by the initial values of \a region_base and \a region_size. The red
//!     zone will only be allowed to extend out of the region described by
//!     these initial values if the user tag is appropriate for stack memory
//!     and the expanded region has the same user tag value.
void LocateRedZone(vm_address_t* const start_address,
                   vm_address_t* const region_base,
                   vm_address_t* const region_size,
                   const unsigned int user_tag) {
  // x86_64 has a red zone. See AMD64 ABI 0.99.8,
  // https://raw.githubusercontent.com/wiki/hjl-tools/x86-psABI/x86-64-psABI-r252.pdf#page=19,
  // section 3.2.2, “The Stack Frame”.
  // So does ARM64,
  // https://developer.apple.com/library/archive/documentation/Xcode/Conceptual/iPhoneOSABIReference/Articles/ARM64FunctionCallingConventions.html
  // section "Red Zone".
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
      // TODO(justincohen): What to do with errors and warnings?
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

//! \brief Calculates the base address and size of the region used as a
//!     thread’s stack.
//!
//! The region returned by this method may be formed by merging multiple
//! adjacent regions in a process’ memory map if appropriate. The base address
//! of the returned region may be lower than the \a stack_pointer passed in
//! when the ABI mandates a red zone below the stack pointer.
//!
//! \param[in] stack_pointer The stack pointer, referring to the top (lowest
//!     address) of a thread’s stack.
//! \param[out] stack_region_size The size of the memory region used as the
//!     thread’s stack.
//!
//! \return The base address (lowest address) of the memory region used as the
//!     thread’s stack.
vm_address_t CalculateStackRegion(vm_address_t stack_pointer,
                                  vm_size_t* stack_region_size) {
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
    // TODO(justincohen): What to do with errors and warnings?
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
      // TODO(justincohen): What to do with errors and warnings?
    }
  }

  *stack_region_size = total_region_size;
  return region_base;
}

}  // namespace

namespace crashpad {
namespace internal {

// static
void InProcessIntermediatedumpHandler::WriteHeader(
    IOSIntermediatedumpWriter* writer) {
  uint8_t version = 1;
  writer->AddProperty(IntermediateDumpKey::kVersion, &version);
}

// static
void InProcessIntermediatedumpHandler::WriteProcessInfo(
    IOSIntermediatedumpWriter* writer) {
  IOSIntermediatedumpWriter::ScopedMap processMap(
      writer, IntermediateDumpKey::kProcessInfo);

  // Used by pid, parent pid and snapshot time.
  kinfo_proc kern_proc_info;
  int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
  size_t len = sizeof(kern_proc_info);
  if (sysctl(mib, base::size(mib), &kern_proc_info, &len, nullptr, 0) == 0) {
    writer->AddProperty(IntermediateDumpKey::kPID,
                        &kern_proc_info.kp_proc.p_pid);
    writer->AddProperty(IntermediateDumpKey::kParentPID,
                        &kern_proc_info.kp_eproc.e_ppid);
    writer->AddProperty(IntermediateDumpKey::kStartTime,
                        &kern_proc_info.kp_proc.p_starttime);
  } else {
    PLOG(WARNING) << "sysctl kern_proc_info";
  }

  // Used by user time and system time.
  task_basic_info_64 task_basic_info;
  mach_msg_type_number_t task_basic_info_count = TASK_BASIC_INFO_64_COUNT;
  kern_return_t kr = task_info(mach_task_self(),
                               TASK_BASIC_INFO_64,
                               reinterpret_cast<task_info_t>(&task_basic_info),
                               &task_basic_info_count);
  if (kr == KERN_SUCCESS) {
    IOSIntermediatedumpWriter::ScopedMap taskInfo(
        writer, IntermediateDumpKey::kTaskBasicInfo);

    writer->AddProperty(IntermediateDumpKey::kUserTime,
                        &task_basic_info.user_time);
    writer->AddProperty(IntermediateDumpKey::kSystemTime,
                        &task_basic_info.system_time,
                        sizeof(time_value_t));
  } else {
    PLOG(WARNING) << "task_info task_basic_info";
  }

  task_thread_times_info_data_t task_thread_times;
  mach_msg_type_number_t task_thread_times_count = TASK_THREAD_TIMES_INFO_COUNT;
  kr = task_info(mach_task_self(),
                 TASK_THREAD_TIMES_INFO,
                 reinterpret_cast<task_info_t>(&task_thread_times),
                 &task_thread_times_count);
  if (kr == KERN_SUCCESS) {
    IOSIntermediatedumpWriter::ScopedMap taskThreadTimesMap(
        writer, IntermediateDumpKey::kTaskThreadTimes);

    writer->AddProperty(IntermediateDumpKey::kUserTime,
                        &task_thread_times.user_time);
    writer->AddProperty(IntermediateDumpKey::kSystemTime,
                        &task_thread_times.system_time);
  } else {
    PLOG(WARNING) << "task_info task_basic_info";
  }

  timeval snapshot_time;
  if (gettimeofday(&snapshot_time, nullptr) == 0) {
    writer->AddProperty(IntermediateDumpKey::kSnapshotTime, &snapshot_time);
  } else {
    PLOG(WARNING) << "gettimeofday";
  }
}

// static
void InProcessIntermediatedumpHandler::WriteSystemInfo(
    IOSIntermediatedumpWriter* writer,
    const IOSSystemDataCollector& system_data) {
  IOSIntermediatedumpWriter::ScopedMap systemMap(
      writer, IntermediateDumpKey::kSystemInfo);

  std::string machine_description = system_data.MachineDescription();
  writer->AddProperty(IntermediateDumpKey::kMachineDescription,
                      machine_description.c_str(),
                      machine_description.length());
  int os_version_major;
  int os_version_minor;
  int os_version_bugfix;
  system_data.OSVersion(
      &os_version_major, &os_version_minor, &os_version_bugfix);
  writer->AddProperty(IntermediateDumpKey::kOSVersionMajor, &os_version_major);
  writer->AddProperty(IntermediateDumpKey::kOSVersionMinor, &os_version_minor);
  writer->AddProperty(IntermediateDumpKey::kOSVersionBugfix,
                      &os_version_bugfix);
  std::string os_version_build = system_data.Build();
  writer->AddProperty(IntermediateDumpKey::kOSVersionBuild,
                      os_version_build.c_str(),
                      os_version_build.length());

  int cpu_count = system_data.ProcessorCount();
  writer->AddProperty(IntermediateDumpKey::kCpuCount, &cpu_count);
  std::string cpu_vendor = system_data.CPUVendor();
  writer->AddProperty(IntermediateDumpKey::kCpuVendor, cpu_vendor.c_str());

  bool has_daylight_saving_time = system_data.HasDaylightSavingTime();
  writer->AddProperty(IntermediateDumpKey::kHasDaylightSavingTime,
                      &has_daylight_saving_time);
  bool is_daylight_saving_time = system_data.IsDaylightSavingTime();
  writer->AddProperty(IntermediateDumpKey::kIsDaylightSavingTime,
                      &is_daylight_saving_time);
  int standard_offset_seconds = system_data.StandardOffsetSeconds();
  writer->AddProperty(IntermediateDumpKey::kStandardOffsetSeconds,
                      &standard_offset_seconds);
  int daylight_offset_seconds = system_data.DaylightOffsetSeconds();
  writer->AddProperty(IntermediateDumpKey::kDaylightOffsetSeconds,
                      &daylight_offset_seconds);
  std::string standard_name = system_data.StandardName();
  writer->AddProperty(IntermediateDumpKey::kStandardName,
                      standard_name.c_str(),
                      standard_name.length());
  std::string daylight_name = system_data.DaylightName();
  writer->AddProperty(IntermediateDumpKey::kDaylightName,
                      daylight_name.c_str(),
                      daylight_name.length());

  vm_size_t page_size;
  host_page_size(mach_host_self(), &page_size);
  writer->AddProperty(IntermediateDumpKey::kPageSize, &page_size);

  mach_msg_type_number_t host_size =
      sizeof(vm_statistics_data_t) / sizeof(integer_t);
  vm_statistics_data_t vm_stat;
  kern_return_t kr = host_statistics(mach_host_self(),
                                     HOST_VM_INFO,
                                     reinterpret_cast<host_info_t>(&vm_stat),
                                     &host_size);
  if (kr == KERN_SUCCESS) {
    IOSIntermediatedumpWriter::ScopedMap vmStatMap(
        writer, IntermediateDumpKey::kVMStat);

    writer->AddProperty(IntermediateDumpKey::kActive, &vm_stat.active_count);
    writer->AddProperty(IntermediateDumpKey::kInactive,
                        &vm_stat.inactive_count);
    writer->AddProperty(IntermediateDumpKey::kWired, &vm_stat.wire_count);
    writer->AddProperty(IntermediateDumpKey::kFree, &vm_stat.free_count);
  } else {
    PLOG(WARNING) << "host_statistics";
  }

  if (kr != KERN_SUCCESS) {
    // TODO(justincohen): What to do with errors and warnings?
  }
}

// static
void InProcessIntermediatedumpHandler::WriteThreadInfo(
    IOSIntermediatedumpWriter* writer,
    const uint64_t* frames,
    const size_t num_frames) {
  IOSIntermediatedumpWriter::ScopedArray threadArray(
      writer, IntermediateDumpKey::kThreads);

  // Exception thread ID.
  uint64_t exception_thread_id = 0;
  thread_identifier_info identifier_info;
  mach_msg_type_number_t count = THREAD_IDENTIFIER_INFO_COUNT;
  kern_return_t kr =
      thread_info(mach_thread_self(),
                  THREAD_IDENTIFIER_INFO,
                  reinterpret_cast<thread_info_t>(&identifier_info),
                  &count);
  if (kr == KERN_SUCCESS) {
    exception_thread_id = identifier_info.thread_id;
    // TODO(justincohen): What to do with errors and warnings?
  }

  mach_msg_type_number_t thread_count = 0;
  thread_act_array_t threads;
  kr = task_threads(mach_task_self(), &threads, &thread_count);
  if (kr != KERN_SUCCESS) {
    // TODO(justincohen): What to do with errors and warnings?
  }
  for (uint32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
    IOSIntermediatedumpWriter::ScopedMap threadMap(writer);
    thread_t thread = threads[thread_index];

    thread_basic_info basic_info;
    mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
    kr = thread_info(thread,
                     THREAD_BASIC_INFO,
                     reinterpret_cast<thread_info_t>(&basic_info),
                     &count);
    if (kr == KERN_SUCCESS) {
      writer->AddProperty(IntermediateDumpKey::kSuspendCount,
                          &basic_info.suspend_count);
    } else {
      // TODO(justincohen): What to do with errors and warnings?
    }

    thread_precedence_policy precedence;
    count = THREAD_PRECEDENCE_POLICY_COUNT;
    boolean_t get_default = FALSE;
    kr = thread_policy_get(thread,
                           THREAD_PRECEDENCE_POLICY,
                           reinterpret_cast<thread_policy_t>(&precedence),
                           &count,
                           &get_default);
    if (kr == KERN_SUCCESS) {
      writer->AddProperty(IntermediateDumpKey::kPriority,
                          &precedence.importance);
    } else {
      // TODO(justincohen): What to do with errors and warnings?
    }

    // Thread ID.
    uint64_t thread_id;
    thread_identifier_info identifier_info;
    count = THREAD_IDENTIFIER_INFO_COUNT;
    kr = thread_info(thread,
                     THREAD_IDENTIFIER_INFO,
                     reinterpret_cast<thread_info_t>(&identifier_info),
                     &count);
    if (kr == KERN_SUCCESS) {
      thread_id = identifier_info.thread_id;
      writer->AddProperty(IntermediateDumpKey::kThreadID,
                          &identifier_info.thread_id);
      writer->AddProperty(IntermediateDumpKey::kThreadDataAddress,
                          &identifier_info.thread_handle);
    } else {
      // TODO(justincohen): What to do with errors and warnings?
    }

#if defined(ARCH_CPU_ARM64)
    if (num_frames > 0 && exception_thread_id == thread_id) {
      writer->AddProperty(IntermediateDumpKey::kThreadUncaughtNSExceptionFrames,
                          frames,
                          num_frames);
      mach_port_deallocate(mach_task_self(), thread);
      continue;
    }
#endif

#if defined(ARCH_CPU_X86_64)
    x86_thread_state64_t thread_state;
    x86_float_state64_t float_state;
    x86_debug_state64_t debug_state;
    mach_msg_type_number_t thread_state_count = x86_THREAD_STATE64_COUNT;
    mach_msg_type_number_t float_state_count = x86_FLOAT_STATE64_COUNT;
    mach_msg_type_number_t debug_state_count = x86_DEBUG_STATE64_COUNT;
#elif defined(ARCH_CPU_ARM64)
    arm_thread_state64_t thread_state;
    arm_neon_state64_t float_state;
    mach_msg_type_number_t thread_state_count = ARM_THREAD_STATE64_COUNT;
    mach_msg_type_number_t float_state_count = ARM_NEON_STATE64_COUNT;
#endif

    kern_return_t kr =
        thread_get_state(thread,
                         kThreadStateFlavor,
                         reinterpret_cast<thread_state_t>(&thread_state),
                         &thread_state_count);
    if (kr != KERN_SUCCESS) {
      // TODO(justincohen): What to do with errors and warnings?
    }
    writer->AddProperty(IntermediateDumpKey::kThreadState, &thread_state);

    kr = thread_get_state(thread,
                          kFloatStateFlavor,
                          reinterpret_cast<thread_state_t>(&float_state),
                          &float_state_count);
    if (kr != KERN_SUCCESS) {
      // TODO(justincohen): What to do with errors and warnings?
    }
    writer->AddProperty(IntermediateDumpKey::kFloatState, &float_state);

#if defined(ARCH_CPU_X86_64)
    kr = thread_get_state(thread,
                          kDebugStateFlavor,
                          reinterpret_cast<thread_state_t>(&debug_state),
                          &debug_state_count);
    if (kr != KERN_SUCCESS) {
      // TODO(justincohen): What to do with errors and warnings?
    }
    writer->AddProperty(IntermediateDumpKey::kDebugState, &debug_state);
#endif

#if defined(ARCH_CPU_X86_64)
    vm_address_t stack_pointer = thread_state.__rsp;
#elif defined(ARCH_CPU_ARM64)
    vm_address_t stack_pointer = thread_state.__sp;
#endif

    vm_size_t stack_region_size;
    const vm_address_t stack_region_address =
        CalculateStackRegion(stack_pointer, &stack_region_size);
    writer->AddProperty(IntermediateDumpKey::kStackRegionAddress,
                        &stack_region_address);
    writer->AddPropertyBytes(
        IntermediateDumpKey::kStackRegionData,
        reinterpret_cast<const void*>(stack_region_address),
        stack_region_size);

    // Grab extra memory from context.
    CaptureMemoryPointedToByThreadState(writer, thread_state);

    mach_port_deallocate(mach_task_self(), thread);
  }
  vm_deallocate(mach_task_self(),
                reinterpret_cast<vm_address_t>(threads),
                sizeof(thread_t) * thread_count);
}

// static
void InProcessIntermediatedumpHandler::WriteModuleInfo(
    IOSIntermediatedumpWriter* writer) {
#ifndef ARCH_CPU_64_BITS
#error Only 64-bit Mach-O is supported
#endif

  IOSIntermediatedumpWriter::ScopedArray moduleArray(
      writer, IntermediateDumpKey::kModules);

  task_dyld_info_data_t dyld_info;
  mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;

  kern_return_t kr = task_info(mach_task_self(),
                               TASK_DYLD_INFO,
                               reinterpret_cast<task_info_t>(&dyld_info),
                               &count);
  if (kr != KERN_SUCCESS) {
    // TODO(justincohen): What to do with errors and warnings?
  }

  const dyld_all_image_infos* image_infos =
      reinterpret_cast<dyld_all_image_infos*>(dyld_info.all_image_info_addr);

  uint32_t image_count = image_infos->infoArrayCount;
  const dyld_image_info* image_array = image_infos->infoArray;
  for (uint32_t image_index = 0; image_index < image_count; ++image_index) {
    IOSIntermediatedumpWriter::ScopedMap modules(writer);

    const dyld_image_info* image = &image_array[image_index];
    writer->AddProperty(IntermediateDumpKey::kName,
                        image->imageFilePath,
                        strlen(image->imageFilePath));
    uint64_t address = FromPointerCast<uint64_t>(image->imageLoadAddress);
    writer->AddProperty(IntermediateDumpKey::kAddress, &address);
    writer->AddProperty(IntermediateDumpKey::kTimestamp,
                        &image->imageFileModDate);
    WriteModuleInfoAtAddress(writer, address);
  }

  {
    IOSIntermediatedumpWriter::ScopedMap modules(writer);
    writer->AddProperty(IntermediateDumpKey::kName, image_infos->dyldPath);
    uint64_t address =
        FromPointerCast<uint64_t>(image_infos->dyldImageLoadAddress);
    WriteModuleInfoAtAddress(writer, address, true /*isDyld=true*/);
    writer->AddProperty(IntermediateDumpKey::kAddress, &address);
  }
}

// static
void InProcessIntermediatedumpHandler::WriteModuleInfoAtAddress(
    IOSIntermediatedumpWriter* writer,
    uint64_t address,
    bool isDyld) {
  const mach_header_64* header =
      reinterpret_cast<const mach_header_64*>(address);
  if (header->magic != MH_MAGIC_64) {
    // TODO(justincohen): What do we do here?
    return;
  }
  const load_command* command =
      reinterpret_cast<const load_command*>(header + 1);
  // Make sure that the basic load command structure doesn’t overflow the
  // space allotted for load commands, as well as iterating through ncmds.
  vm_size_t slide = 0;
  const symtab_command* symtab_command = nullptr;
  const dysymtab_command* dysymtab_command = nullptr;
  const segment_command_64* linkedit_seg = nullptr;
  const segment_command_64* text_seg = nullptr;
  for (uint32_t cmd_index = 0, cumulative_cmd_size = 0;
       cmd_index <= header->ncmds && cumulative_cmd_size < header->sizeofcmds;
       ++cmd_index, cumulative_cmd_size += command->cmdsize) {
    if (command->cmd == LC_SEGMENT_64) {
      const segment_command_64* segment =
          reinterpret_cast<const segment_command_64*>(command);
      if (strcmp(segment->segname, SEG_TEXT) == 0) {
        text_seg = segment;
        writer->AddProperty(IntermediateDumpKey::kSize, &segment->vmsize);
        slide = address - segment->vmaddr;
      } else if (strcmp(segment->segname, SEG_DATA) == 0) {
        WriteDataAnnotations(writer, segment, slide);
      } else if (strcmp(segment->segname, SEG_LINKEDIT) == 0) {
        linkedit_seg = segment;
      }
    } else if (command->cmd == LC_SYMTAB) {
      symtab_command = reinterpret_cast<const struct symtab_command*>(command);
    } else if (command->cmd == LC_DYSYMTAB) {
      dysymtab_command =
          reinterpret_cast<const struct dysymtab_command*>(command);
    } else if (command->cmd == LC_ID_DYLIB) {
      const dylib_command* dylib =
          reinterpret_cast<const dylib_command*>(command);
      writer->AddProperty(IntermediateDumpKey::kDylibCurrentVersion,
                          &dylib->dylib.current_version);
    } else if (command->cmd == LC_SOURCE_VERSION) {
      const source_version_command* source_version =
          reinterpret_cast<const source_version_command*>(command);
      writer->AddProperty(IntermediateDumpKey::kSourceVersion,
                          &source_version->version);
    } else if (command->cmd == LC_UUID) {
      const uuid_command* uuid = reinterpret_cast<const uuid_command*>(command);
      writer->AddProperty(IntermediateDumpKey::kUUID, &uuid->uuid);
    }

    command = reinterpret_cast<const load_command*>(
        reinterpret_cast<const uint8_t*>(command) + command->cmdsize);

    // TODO(justincohen): Warn-able things:
    // - Bad Mach-O magic (and give up trying to process the module)
    // - Unrecognized Mach-O type
    // - No SEG_TEXT
    // - More than one SEG_TEXT
    // - More than one LC_ID_DYLIB, LC_SOURCE_VERSION, or LC_UUID
    // - No LC_ID_DYLIB in a dylib file
    // - LC_ID_DYLIB in a non-dylib file
    // And more optional:
    // - Missing LC_UUID (although it leaves us with a big "?")
    // - Missing LC_SOURCE_VERSION.
  }

  writer->AddProperty(IntermediateDumpKey::kFileType, &header->filetype);

  if (isDyld && header->filetype == MH_DYLINKER) {
    WriteDyldErrorStringAnnotation(writer,
                                   address,
                                   symtab_command,
                                   dysymtab_command,
                                   text_seg,
                                   linkedit_seg,
                                   slide);
  }
}

// static
void InProcessIntermediatedumpHandler::WriteDataAnnotations(
    IOSIntermediatedumpWriter* writer,
    const segment_command_64* segment,
    vm_size_t slide) {
  const section_64* section = reinterpret_cast<const section_64*>(
      reinterpret_cast<uint64_t>(segment) + sizeof(segment_command_64));
  for (uint32_t sect_index = 0; sect_index <= segment->nsects; ++sect_index) {
    if (strcmp(section->sectname, "crashpad_info") == 0) {
      ScopedVMRead<process_types::CrashpadInfo> crashpad_info(section->addr +
                                                              slide);
      if (crashpad_info.is_valid() &&
          crashpad_info->signature == CrashpadInfo::kSignature &&
          crashpad_info->version == 1) {
        WriteAnnotationList(writer, crashpad_info.get());
        WriteSimpleAnnotation(writer, crashpad_info.get());
      }
    }
    if (strcmp(section->sectname, "__crash_info") == 0) {
      ScopedVMRead<process_types::crashreporter_annotations_t> crash_info(
          section->addr + slide);
      if (!crash_info.is_valid() ||
          (crash_info->version != 4 && crash_info->version != 5)) {
        continue;
      }
      WriteCrashInfoAnnotations(writer, crash_info.get());
    }
    section = reinterpret_cast<const section_64*>(
        reinterpret_cast<uint64_t>(section) + sizeof(section_64));
  }
}

// static
void InProcessIntermediatedumpHandler::WriteMachExceptionInfo(
    IOSIntermediatedumpWriter* writer,
    exception_behavior_t behavior,
    thread_t exception_thread,
    exception_type_t exception,
    const mach_exception_data_type_t* code,
    mach_msg_type_number_t code_count,
    thread_state_flavor_t flavor,
    ConstThreadState state,
    mach_msg_type_number_t state_count) {
  IOSIntermediatedumpWriter::ScopedMap machExceptionMap(
      writer, IntermediateDumpKey::kMachException);

  writer->AddProperty(IntermediateDumpKey::kException, &exception);
  writer->AddProperty(IntermediateDumpKey::kCode, code, code_count);
  writer->AddProperty(IntermediateDumpKey::kCodeCount, &code_count);
  writer->AddProperty(IntermediateDumpKey::kFlavor, &flavor);
  size_t state_length =
      IOSIntermediatedumpWriter::ThreadStateLengthForFlavor(flavor);
  writer->AddPropertyBytes(IntermediateDumpKey::kState, state, state_length);
  writer->AddProperty(IntermediateDumpKey::kStateCount, &state_count);

  // Thread ID.
  thread_identifier_info identifier_info;
  mach_msg_type_number_t count = THREAD_IDENTIFIER_INFO_COUNT;
  kern_return_t kr =
      thread_info(exception_thread,
                  THREAD_IDENTIFIER_INFO,
                  reinterpret_cast<thread_info_t>(&identifier_info),
                  &count);
  if (kr == KERN_SUCCESS) {
    writer->AddProperty(IntermediateDumpKey::kThreadID,
                        &identifier_info.thread_id);
  } else {
    // TODO(justincohen): What to do with errors and warnings?
  }
}

// static
void InProcessIntermediatedumpHandler::WriteNSException(
    IOSIntermediatedumpWriter* writer) {
  IOSIntermediatedumpWriter::ScopedMap NSExceptionMap(
      writer, IntermediateDumpKey::kNSException);

  thread_identifier_info identifier_info;
  mach_msg_type_number_t count = THREAD_IDENTIFIER_INFO_COUNT;
  kern_return_t kr =
      thread_info(mach_thread_self(),
                  THREAD_IDENTIFIER_INFO,
                  reinterpret_cast<thread_info_t>(&identifier_info),
                  &count);
  if (kr == KERN_SUCCESS) {
    writer->AddProperty(IntermediateDumpKey::kThreadID,
                        &identifier_info.thread_id);
  } else {
    // TODO(justincohen): What to do with errors and warnings?
  }
}

// static
void InProcessIntermediatedumpHandler::MaybeCaptureMemoryAround(
    IOSIntermediatedumpWriter* writer,
    uint64_t address) {
  constexpr uint64_t non_address_offset = 0x10000;
  if (address < non_address_offset)
    return;

  const uint64_t max_address = std::numeric_limits<uint64_t>::max();

  if (address > max_address - non_address_offset)
    return;

  constexpr uint64_t kRegisterByteOffset = 128;
  const uint64_t target = address - kRegisterByteOffset;
  constexpr uint64_t size = 512;
  static_assert(kRegisterByteOffset <= size / 2, "negative offset too large");

  IOSIntermediatedumpWriter::ScopedMap memoryRegion(writer);
  writer->AddProperty(IntermediateDumpKey::kThreadContextMemoryRegionAddress,
                      &address);
  writer->AddPropertyBytes(IntermediateDumpKey::kThreadContextMemoryRegionData,
                           reinterpret_cast<const void*>(target),
                           size);
}

// static
void InProcessIntermediatedumpHandler::CaptureMemoryPointedToByThreadState(
    IOSIntermediatedumpWriter* writer,
    thread_state_type thread_state) {
  IOSIntermediatedumpWriter::ScopedArray memoryRegions(
      writer, IntermediateDumpKey::kThreadContextMemoryRegions);

#if defined(ARCH_CPU_X86_64)
  MaybeCaptureMemoryAround(writer, thread_state.__rax);
  MaybeCaptureMemoryAround(writer, thread_state.__rbx);
  MaybeCaptureMemoryAround(writer, thread_state.__rcx);
  MaybeCaptureMemoryAround(writer, thread_state.__rdx);
  MaybeCaptureMemoryAround(writer, thread_state.__rdi);
  MaybeCaptureMemoryAround(writer, thread_state.__rsi);
  MaybeCaptureMemoryAround(writer, thread_state.__rbp);
  MaybeCaptureMemoryAround(writer, thread_state.__r8);
  MaybeCaptureMemoryAround(writer, thread_state.__r9);
  MaybeCaptureMemoryAround(writer, thread_state.__r10);
  MaybeCaptureMemoryAround(writer, thread_state.__r11);
  MaybeCaptureMemoryAround(writer, thread_state.__r12);
  MaybeCaptureMemoryAround(writer, thread_state.__r13);
  MaybeCaptureMemoryAround(writer, thread_state.__r14);
  MaybeCaptureMemoryAround(writer, thread_state.__r15);
  MaybeCaptureMemoryAround(writer, thread_state.__rip);
#elif defined(ARCH_CPU_ARM_FAMILY)
  MaybeCaptureMemoryAround(writer, thread_state.__pc);
  for (size_t i = 0; i < base::size(thread_state.__x); ++i) {
    MaybeCaptureMemoryAround(writer, thread_state.__x[i]);
  }
#endif
}

// static
void InProcessIntermediatedumpHandler::WriteExceptionFromSignal(
    IOSIntermediatedumpWriter* writer,
    const IOSSystemDataCollector& system_data,
    siginfo_t* siginfo,
    ucontext_t* context) {
  IOSIntermediatedumpWriter::ScopedMap signalExceptionMap(
      writer, IntermediateDumpKey::kSignalException);

  writer->AddProperty(IntermediateDumpKey::kSignalNumber, &siginfo->si_signo);
  writer->AddProperty(IntermediateDumpKey::kSignalCode, &siginfo->si_code);
  writer->AddProperty(IntermediateDumpKey::kSignalAddress, &siginfo->si_addr);
#if defined(ARCH_CPU_X86_64)
  writer->AddProperty(IntermediateDumpKey::kThreadState,
                      &context->uc_mcontext->__ss);
  writer->AddProperty(IntermediateDumpKey::kFloatState,
                      &context->uc_mcontext->__fs);
#elif defined(ARCH_CPU_ARM64)
  writer->AddProperty(IntermediateDumpKey::kThreadState,
                      &context->uc_mcontext->__ss);
  writer->AddProperty(IntermediateDumpKey::kFloatState,
                      &context->uc_mcontext->__ns);
#else
#error Port to your CPU architecture
#endif

  // Thread ID.
  thread_identifier_info identifier_info;
  mach_msg_type_number_t count = THREAD_IDENTIFIER_INFO_COUNT;
  kern_return_t kr =
      thread_info(mach_thread_self(),
                  THREAD_IDENTIFIER_INFO,
                  reinterpret_cast<thread_info_t>(&identifier_info),
                  &count);
  if (kr == KERN_SUCCESS) {
    writer->AddProperty(IntermediateDumpKey::kThreadID,
                        &identifier_info.thread_id);
  } else {
    // TODO(justincohen): What to do with errors and warnings?
  }
}

// static
void InProcessIntermediatedumpHandler::WriteAnnotationList(
    IOSIntermediatedumpWriter* writer,
    process_types::CrashpadInfo* crashpad_info) {
  if (!crashpad_info->annotations_list) {
    return;
  }
  ScopedVMRead<process_types::AnnotationList> annotation_list(
      crashpad_info->annotations_list);
  if (!annotation_list.is_valid()) {
    LOG(WARNING) << "could not read annotations list object";
    return;
  }

  IOSIntermediatedumpWriter::ScopedArray annotationsArray(
      writer, IntermediateDumpKey::kAnnotationObjects);
  ScopedVMRead<process_types::Annotation> current(&annotation_list->head);
  if (!current.is_valid()) {
    LOG(WARNING) << "could not read annotation";
    return;
  }

  for (size_t index = 0;
       current->link_node != annotation_list.get()->tail_pointer &&
       index < kMaxNumberOfAnnotations;
       ++index) {
    ScopedVMRead<process_types::Annotation> node(current->link_node);
    if (!node.is_valid()) {
      LOG(WARNING) << "could not read annotation";
      return;
    }
    current.reset(
        reinterpret_cast<process_types::Annotation*>(current->link_node));

    if (node->size == 0)
      continue;

    if (node->size > Annotation::kValueMaxSize) {
      DLOG(WARNING) << "Incorrect annotation length " << node->size;
      continue;
    }

    IOSIntermediatedumpWriter::ScopedMap annotationMap(writer);
    const size_t name_len =
        strnlen((const char*)node->name, Annotation::kNameMaxLength);
    writer->AddPropertyBytes(IntermediateDumpKey::kAnnotationName,
                             reinterpret_cast<const void*>(node->name),
                             name_len);
    writer->AddPropertyBytes(IntermediateDumpKey::kAnnotationValue,
                             reinterpret_cast<const void*>(node->value),
                             node->size);
    writer->AddPropertyBytes(IntermediateDumpKey::kAnnotationType,
                             reinterpret_cast<const void*>(&node->type),
                             sizeof(node->type));
  }
}

// static
void InProcessIntermediatedumpHandler::WriteSimpleAnnotation(
    IOSIntermediatedumpWriter* writer,
    process_types::CrashpadInfo* crashpad_info) {
  if (!crashpad_info->simple_annotations)
    return;

  ScopedVMRead<SimpleStringDictionary> simple_annotations(
      crashpad_info->simple_annotations, SimpleStringDictionary::num_entries);
  if (!simple_annotations.is_valid()) {
    LOG(WARNING) << "could not read simple annotations.";
    return;
  }

  const size_t count = simple_annotations->GetCount();
  if (!count)
    return;

  IOSIntermediatedumpWriter::ScopedArray annotationsArray(
      writer, IntermediateDumpKey::kAnnotationsSimpleMap);

  SimpleStringDictionary::Entry* entries =
      reinterpret_cast<SimpleStringDictionary::Entry*>(
          simple_annotations.get());
  for (size_t index = 0; index < count; index++) {
    IOSIntermediatedumpWriter::ScopedMap annotationMap(writer);
    const auto& entry = entries[index];
    size_t key_length = strnlen(entry.key, sizeof(entry.key));
    writer->AddPropertyBytes(IntermediateDumpKey::kAnnotationName,
                             reinterpret_cast<const void*>(entry.key),
                             key_length);
    size_t value_length = strnlen(entry.value, sizeof(entry.value));
    writer->AddPropertyBytes(IntermediateDumpKey::kAnnotationValue,
                             reinterpret_cast<const void*>(entry.value),
                             value_length);
  }
}

// static
void InProcessIntermediatedumpHandler::WriteCrashInfoAnnotations(
    IOSIntermediatedumpWriter* writer,
    process_types::crashreporter_annotations_t* crash_info) {
  // This number was totally made up out of nowhere, but it seems prudent to
  // enforce some limit.
  constexpr size_t kMaxMessageSize = 1024;
  IOSIntermediatedumpWriter::ScopedMap annotationMap(
      writer, IntermediateDumpKey::kAnnotationsCrashInfo);
  if (crash_info->message) {
    const size_t message_len =
        strnlen((const char*)crash_info->message, kMaxMessageSize);
    writer->AddPropertyBytes(IntermediateDumpKey::kAnnotationsCrashInfoMessage1,
                             reinterpret_cast<const void*>(crash_info->message),
                             message_len);
  }
  if (crash_info->message2) {
    const size_t message_len =
        strnlen((const char*)crash_info->message2, kMaxMessageSize);
    writer->AddPropertyBytes(
        IntermediateDumpKey::kAnnotationsCrashInfoMessage2,
        reinterpret_cast<const void*>(crash_info->message2),
        message_len);
  }
}

// static
void InProcessIntermediatedumpHandler::WriteDyldErrorStringAnnotation(
    IOSIntermediatedumpWriter* writer,
    const uint64_t address,
    const symtab_command* symtab_command,
    const dysymtab_command* dysymtab_command,
    const segment_command_64* text_seg,
    const segment_command_64* linkedit_seg,
    vm_size_t slide) {
  if (text_seg == nullptr || linkedit_seg == nullptr ||
      symtab_command == nullptr) {
    return;
  }

  uint64_t file_slide =
      (linkedit_seg->vmaddr - text_seg->vmaddr) - linkedit_seg->fileoff;
  uint64_t strings = address + (symtab_command->stroff + file_slide);
  nlist_64* symbol = reinterpret_cast<nlist_64*>(
      address + (symtab_command->symoff + file_slide));

  // If a dysymtab is present, use it to filter the symtab for just the
  // portion used for extdefsym. If no dysymtab is present, the entire symtab
  // will need to be consulted.
  uint32_t symbol_count = symtab_command->nsyms;
  if (dysymtab_command) {
    symbol += dysymtab_command->iextdefsym;
    symbol_count = dysymtab_command->nextdefsym;
  }

  for (uint32_t i = 0; i < symbol_count; i++, symbol++) {
    if (!symbol->n_value)
      continue;

    if (strcmp(reinterpret_cast<const char*>(strings + symbol->n_un.n_strx),
               "_error_string") == 0) {
      const char* value =
          reinterpret_cast<const char*>(symbol->n_value + slide);
      // 1024 here is distinct from kMaxMessageSize above, because it refers to
      // a precisely-sized buffer inside dyld.
      const size_t value_len = strnlen(value, 1024);
      if (value_len) {
        writer->AddProperty(
            IntermediateDumpKey::kAnnotationsDyldErrorString, value, value_len);
      }
      return;
    }

    continue;
  }
}

}  // namespace internal
}  // namespace crashpad
