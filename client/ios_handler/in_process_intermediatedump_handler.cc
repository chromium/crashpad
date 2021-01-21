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
#include <mach-o/loader.h>
#include <mach/mach.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <uuid/uuid.h>

#include "base/logging.h"
#include "client/crashpad_info.h"
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
  writer->AddProperty(IntermediateDumpKey::kVersion, &version, sizeof(version));
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
                        &kern_proc_info.kp_proc.p_pid,
                        sizeof(pid_t));
    writer->AddProperty(IntermediateDumpKey::kParentPID,
                        &kern_proc_info.kp_eproc.e_ppid,
                        sizeof(pid_t));
    writer->AddProperty(IntermediateDumpKey::kStartTime,
                        &kern_proc_info.kp_proc.p_starttime,
                        sizeof(timeval));
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
                        &task_basic_info.user_time,
                        sizeof(time_value_t));
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
                        &task_thread_times.user_time,
                        sizeof(time_value_t));
    writer->AddProperty(IntermediateDumpKey::kSystemTime,
                        &task_thread_times.system_time,
                        sizeof(time_value_t));
  } else {
    PLOG(WARNING) << "task_info task_basic_info";
  }

  timeval snapshot_time;
  if (gettimeofday(&snapshot_time, nullptr) == 0) {
    writer->AddProperty(IntermediateDumpKey::kSnapshotTime,
                        &snapshot_time,
                        sizeof(snapshot_time));
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
  std::string os_version_build = system_data.Build();
  writer->AddProperty(IntermediateDumpKey::kOSVersionMajor,
                      &os_version_major,
                      sizeof(os_version_major));
  writer->AddProperty(IntermediateDumpKey::kOSVersionMinor,
                      &os_version_minor,
                      sizeof(os_version_minor));
  writer->AddProperty(IntermediateDumpKey::kOSVersionBugfix,
                      &os_version_bugfix,
                      sizeof(os_version_bugfix));
  writer->AddProperty(IntermediateDumpKey::kOSVersionBuild,
                      os_version_build.c_str(),
                      os_version_build.length());

  int cpu_count = system_data.ProcessorCount();
  writer->AddProperty(
      IntermediateDumpKey::kCpuCount, &cpu_count, sizeof(cpu_count));
  std::string cpu_vendor = system_data.CPUVendor();
  writer->AddProperty(
      IntermediateDumpKey::kCpuVendor, cpu_vendor.c_str(), cpu_vendor.length());

  bool has_daylight_saving_time = system_data.HasDaylightSavingTime();
  writer->AddProperty(IntermediateDumpKey::kHasDaylightSavingTime,
                      &has_daylight_saving_time,
                      sizeof(has_daylight_saving_time));
  bool is_daylight_saving_time = system_data.IsDaylightSavingTime();
  writer->AddProperty(IntermediateDumpKey::kIsDaylightSavingTime,
                      &is_daylight_saving_time,
                      sizeof(is_daylight_saving_time));
  int standard_offset_seconds = system_data.StandardOffsetSeconds();
  writer->AddProperty(IntermediateDumpKey::kStandardOffsetSeconds,
                      &standard_offset_seconds,
                      sizeof(standard_offset_seconds));
  int daylight_offset_seconds = system_data.DaylightOffsetSeconds();
  writer->AddProperty(IntermediateDumpKey::kDaylightOffsetSeconds,
                      &daylight_offset_seconds,
                      sizeof(daylight_offset_seconds));
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
  writer->AddProperty(
      IntermediateDumpKey::kPageSize, &page_size, sizeof(page_size));

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

    writer->AddProperty(
        IntermediateDumpKey::kActive, &vm_stat.active_count, sizeof(uint64_t));
    writer->AddProperty(IntermediateDumpKey::kInactive,
                        &vm_stat.inactive_count,
                        sizeof(uint64_t));
    writer->AddProperty(
        IntermediateDumpKey::kWired, &vm_stat.wire_count, sizeof(uint64_t));
    writer->AddProperty(
        IntermediateDumpKey::kFree, &vm_stat.free_count, sizeof(uint64_t));
  } else {
    PLOG(WARNING) << "host_statistics";
  }

  if (kr != KERN_SUCCESS) {
    // TODO(justincohen): What to do with errors and warnings?
  }
}

// static
void InProcessIntermediatedumpHandler::WriteThreadInfo(
    IOSIntermediatedumpWriter* writer) {
  IOSIntermediatedumpWriter::ScopedArray threadArray(
      writer, IntermediateDumpKey::kThreads);

  mach_msg_type_number_t thread_count = 0;
  thread_act_array_t threads;
  kern_return_t kr = task_threads(mach_task_self(), &threads, &thread_count);
  if (kr != KERN_SUCCESS) {
    // TODO(justincohen): What to do with errors and warnings?
  }
  for (uint32_t thread_index = 0; thread_index < thread_count; ++thread_index) {
    IOSIntermediatedumpWriter::ScopedMap threadMap(writer);

    thread_t thread = threads[thread_index];

    thread_basic_info basic_info;
    thread_precedence_policy precedence;
    vm_size_t stack_region_size;
    vm_address_t stack_region_address;
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
    writer->AddProperty(
        IntermediateDumpKey::kThreadState, &thread_state, sizeof(thread_state));

    kr = thread_get_state(thread,
                          kFloatStateFlavor,
                          reinterpret_cast<thread_state_t>(&float_state),
                          &float_state_count);
    if (kr != KERN_SUCCESS) {
      // TODO(justincohen): What to do with errors and warnings?
    }
    writer->AddProperty(
        IntermediateDumpKey::kFloatState, &float_state, sizeof(float_state));

#if defined(ARCH_CPU_X86_64)
    kr = thread_get_state(thread,
                          kDebugStateFlavor,
                          reinterpret_cast<thread_state_t>(&debug_state),
                          &debug_state_count);
    if (kr != KERN_SUCCESS) {
      // TODO(justincohen): What to do with errors and warnings?
    }
    writer->AddProperty(
        IntermediateDumpKey::kDebugState, &debug_state, sizeof(debug_state));
#endif

    mach_msg_type_number_t count = THREAD_BASIC_INFO_COUNT;
    kr = thread_info(thread,
                     THREAD_BASIC_INFO,
                     reinterpret_cast<thread_info_t>(&basic_info),
                     &count);
    if (kr == KERN_SUCCESS) {
      writer->AddProperty(IntermediateDumpKey::kSuspendCount,
                          &basic_info.suspend_count,
                          sizeof(int));
    } else {
      // TODO(justincohen): What to do with errors and warnings?
    }

    thread_identifier_info identifier_info;
    count = THREAD_IDENTIFIER_INFO_COUNT;
    kr = thread_info(thread,
                     THREAD_IDENTIFIER_INFO,
                     reinterpret_cast<thread_info_t>(&identifier_info),
                     &count);
    if (kr == KERN_SUCCESS) {
      writer->AddProperty(IntermediateDumpKey::kThreadID,
                          &identifier_info.thread_id,
                          sizeof(uint64_t));
      writer->AddProperty(IntermediateDumpKey::kThreadDataAddress,
                          &identifier_info.thread_handle,
                          sizeof(uint64_t));
    } else {
      // TODO(justincohen): What to do with errors and warnings?
    }

    count = THREAD_PRECEDENCE_POLICY_COUNT;
    boolean_t get_default = FALSE;
    kr = thread_policy_get(thread,
                           THREAD_PRECEDENCE_POLICY,
                           reinterpret_cast<thread_policy_t>(&precedence),
                           &count,
                           &get_default);
    if (kr == KERN_SUCCESS) {
      writer->AddProperty(
          IntermediateDumpKey::kPriority, &precedence.importance, sizeof(int));
    } else {
      // TODO(justincohen): What to do with errors and warnings?
    }

#if defined(ARCH_CPU_X86_64)
    vm_address_t stack_pointer = thread_state.__rsp;
#elif defined(ARCH_CPU_ARM64)
    vm_address_t stack_pointer = thread_state.__sp;
#endif

    // TODO fix this.
    stack_region_address =
        CalculateStackRegion(stack_pointer, &stack_region_size);
    writer->AddProperty(IntermediateDumpKey::kStackRegionAddress,
                        &stack_region_address,
                        sizeof(stack_region_address));

    mach_vm_address_t page_region_address =
        mach_vm_trunc_page(stack_region_address);
    mach_vm_size_t page_region_size = mach_vm_round_page(
        stack_region_address - page_region_address + stack_region_size);

    vm_address_t data;
    mach_msg_type_number_t dataCnt = 0;
    kr = vm_read(mach_task_self(),
                 page_region_address,
                 page_region_size,
                 &data,
                 &dataCnt);
    if (kr == KERN_SUCCESS) {
      const void* offset_data = reinterpret_cast<const void*>(
          data + (stack_region_address - page_region_address));
      writer->AddProperty(IntermediateDumpKey::kStackRegionData,
                          offset_data,
                          stack_region_size);
      vm_deallocate(mach_task_self(), data, dataCnt);
    }

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
    writer->AddProperty(
        IntermediateDumpKey::kAddress, &address, sizeof(address));
    writer->AddProperty(IntermediateDumpKey::kTimestamp,
                        &image->imageFileModDate,
                        sizeof(image->imageFileModDate));
    WriteModuleInfoAtAddress(writer, address);
  }

  {
    IOSIntermediatedumpWriter::ScopedMap modules(writer);
    writer->AddProperty(IntermediateDumpKey::kName,
                        image_infos->dyldPath,
                        strlen(image_infos->dyldPath));
    uint64_t address =
        FromPointerCast<uint64_t>(image_infos->dyldImageLoadAddress);
    WriteModuleInfoAtAddress(writer, address);
    writer->AddProperty(
        IntermediateDumpKey::kAddress, &address, sizeof(address));
  }
}

// static
void InProcessIntermediatedumpHandler::WriteModuleInfoAtAddress(
    IOSIntermediatedumpWriter* writer,
    uint64_t address) {
  const mach_header_64* header =
      reinterpret_cast<const mach_header_64*>(address);
  const load_command* command =
      reinterpret_cast<const load_command*>(header + 1);
  // Make sure that the basic load command structure doesn’t overflow the
  // space allotted for load commands, as well as iterating through ncmds.
  vm_size_t slide = 0;
  for (uint32_t cmd_index = 0, cumulative_cmd_size = 0;
       cmd_index <= header->ncmds && cumulative_cmd_size < header->sizeofcmds;
       ++cmd_index, cumulative_cmd_size += command->cmdsize) {
    if (command->cmd == LC_SEGMENT_64) {
      const segment_command_64* segment =
          reinterpret_cast<const segment_command_64*>(command);
      if (strcmp(segment->segname, SEG_TEXT) == 0) {
        writer->AddProperty(IntermediateDumpKey::kSize,
                            &segment->vmsize,
                            sizeof(segment->vmsize));
        slide = address - segment->vmaddr;
      } else if (strcmp(segment->segname, SEG_DATA) == 0) {
        const section_64* section = reinterpret_cast<const section_64*>(
            reinterpret_cast<uint64_t>(segment) + sizeof(segment_command_64));
        for (uint32_t sect_index = 0; sect_index <= segment->nsects;
             ++sect_index) {
          if (strcmp(section->sectname, "crashpad_info") == 0) {
            const CrashpadInfo* crashpad_info =
                reinterpret_cast<const CrashpadInfo*>(section->addr + slide);
            if (crashpad_info && crashpad_info->is_valid()) {
              AnnotationList* annotations_list =
                  crashpad_info->annotations_list();
              if (annotations_list) {
                IOSIntermediatedumpWriter::ScopedArray annotationsArray(
                    writer, IntermediateDumpKey::kAnnotationsList);
                for (AnnotationList::Iterator it = annotations_list->begin();
                     it != annotations_list->end();
                     ++it) {
                  IOSIntermediatedumpWriter::ScopedMap annotationMap(writer);
                  Annotation* annotation = nullptr;
                  IOSIntermediatedumpWriter::ScopedVMRead data(
                      *it, sizeof(Annotation), &annotation);
                  if (!annotation) {
                    DLOG(ERROR) << "Invalid annotation";
                    break;
                  }

                  if (annotation->size() <= Annotation::kValueMaxSize) {
                    const size_t name_len =
                        strnlen(annotation->name(), Annotation::kNameMaxLength);
                    writer->AddProperty(IntermediateDumpKey::kAnnotationName,
                                        annotation->name(),
                                        name_len);
                    writer->AddProperty(IntermediateDumpKey::kAnnotationValue,
                                        annotation->value(),
                                        annotation->size());
                    Annotation::Type annotation_type = annotation->type();
                    writer->AddProperty(IntermediateDumpKey::kAnnotationType,
                                        &annotation_type,
                                        sizeof(annotation->type()));
                  } else {
                    DLOG(ERROR) << "Got a bad annotation length";
                    break;
                  }
                }
              }
            }
          }
          section = reinterpret_cast<const section_64*>(
              reinterpret_cast<uint64_t>(section) + sizeof(section_64));
        }
      }
    } else if (command->cmd == LC_ID_DYLIB) {
      const dylib_command* dylib =
          reinterpret_cast<const dylib_command*>(command);
      writer->AddProperty(IntermediateDumpKey::kDylibCurrentVersion,
                          &dylib->dylib.current_version,
                          sizeof(dylib->dylib.current_version));
    } else if (command->cmd == LC_SOURCE_VERSION) {
      const source_version_command* source_version =
          reinterpret_cast<const source_version_command*>(command);
      writer->AddProperty(IntermediateDumpKey::kSourceVersion,
                          &source_version->version,
                          sizeof(source_version->version));
    } else if (command->cmd == LC_UUID) {
      const uuid_command* uuid = reinterpret_cast<const uuid_command*>(command);
      writer->AddProperty(
          IntermediateDumpKey::kUUID, &uuid->uuid, sizeof(uuid->uuid));
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

  writer->AddProperty(IntermediateDumpKey::kFileType,
                      &header->filetype,
                      sizeof(header->filetype));
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

  writer->AddProperty(
      IntermediateDumpKey::kException, &exception, sizeof(exception));
  writer->AddProperty(IntermediateDumpKey::kCode,
                      &code,
                      sizeof(mach_exception_data_type_t) * code_count);
  writer->AddProperty(IntermediateDumpKey::kCodeCount,
                      &code_count,
                      sizeof(mach_msg_type_number_t));
  writer->AddProperty(IntermediateDumpKey::kFlavor, &flavor, sizeof(flavor));
  size_t state_length =
      IOSIntermediatedumpWriter::ThreadStateLengthForFlavor(flavor);
  writer->AddProperty(IntermediateDumpKey::kState, state, state_length);
  writer->AddProperty(
      IntermediateDumpKey::kStateCount, &state_count, sizeof(state_count));

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
                        &identifier_info.thread_id,
                        sizeof(uint64_t));
  } else {
    // TODO(justincohen): What to do with errors and warnings?
  }
}

// static
void InProcessIntermediatedumpHandler::WriteExceptionFromSignal(
    IOSIntermediatedumpWriter* writer,
    const IOSSystemDataCollector& system_data,
    siginfo_t* siginfo,
    ucontext_t* context) {
  IOSIntermediatedumpWriter::ScopedMap signalExceptionMap(
      writer, IntermediateDumpKey::kSignalException);

  writer->AddProperty(IntermediateDumpKey::kSignalNumber,
                      &siginfo->si_signo,
                      sizeof(siginfo->si_signo));
  writer->AddProperty(IntermediateDumpKey::kSignalCode,
                      &siginfo->si_code,
                      sizeof(siginfo->si_code));
  writer->AddProperty(IntermediateDumpKey::kSignalAddress,
                      &siginfo->si_addr,
                      sizeof(siginfo->si_addr));
#if defined(ARCH_CPU_X86_64)
  writer->AddProperty(IntermediateDumpKey::kThreadState,
                      &context->uc_mcontext->__ss,
                      sizeof(x86_thread_state64_t));
  writer->AddProperty(IntermediateDumpKey::kFloatState,
                      &context->uc_mcontext->__fs,
                      sizeof(x86_float_state64_t));
#elif defined(ARCH_CPU_ARM64)
  writer->AddProperty(IntermediateDumpKey::kThreadState,
                      &context->uc_mcontext->__ss,
                      sizeof(arm_thread_state64_t));
  writer->AddProperty(IntermediateDumpKey::kFloatState,
                      &context->uc_mcontext->__ns,
                      sizeof(arm_neon_state64_t));
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
                        &identifier_info.thread_id,
                        sizeof(uint64_t));
  } else {
    // TODO(justincohen): What to do with errors and warnings?
  }
}

}  // namespace internal
}  // namespace crashpad
