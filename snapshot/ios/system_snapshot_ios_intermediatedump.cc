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

#include "snapshot/ios/system_snapshot_ios_intermediatedump.h"

#include <mach/mach.h>
#include <stddef.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/utsname.h>

#include <algorithm>

#include "base/logging.h"
#include "base/mac/mach_logging.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "snapshot/cpu_context.h"
#include "snapshot/posix/timezone.h"
#include "util/ios/ios_intermediatedump_data.h"
#include "util/mac/mac_util.h"
#include "util/numeric/in_range_cast.h"

namespace crashpad {

namespace internal {

SystemSnapshotIOSIntermediatedump::SystemSnapshotIOSIntermediatedump()
    : SystemSnapshot(),
      os_version_build_(),
      machine_description_(),
      os_version_major_(0),
      os_version_minor_(0),
      os_version_bugfix_(0),
      active_(0),
      inactive_(0),
      wired_(0),
      free_(0),
      cpu_count_(0),
      cpu_vendor_(),
      dst_status_(),
      standard_offset_seconds_(0),
      daylight_offset_seconds_(0),
      standard_name_(),
      daylight_name_(),
      initialized_() {}

SystemSnapshotIOSIntermediatedump::~SystemSnapshotIOSIntermediatedump() {}

void SystemSnapshotIOSIntermediatedump::Initialize(
    const IOSIntermediatedumpMap& system_data) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);
  system_data[IntermediateDumpKey::kOSVersionBuild].GetString(
      &os_version_build_);
  system_data[IntermediateDumpKey::kMachineDescription].GetString(
      &machine_description_);
  system_data[IntermediateDumpKey::kCpuVendor].GetString(&cpu_vendor_);
  system_data[IntermediateDumpKey::kStandardName].GetString(&standard_name_);
  system_data[IntermediateDumpKey::kDaylightName].GetString(&daylight_name_);

  system_data[IntermediateDumpKey::kOSVersionMajor].GetInt(&os_version_major_);
  system_data[IntermediateDumpKey::kOSVersionMinor].GetInt(&os_version_minor_);
  system_data[IntermediateDumpKey::kOSVersionBugfix].GetInt(
      &os_version_bugfix_);
  system_data[IntermediateDumpKey::kCpuCount].GetInt(&cpu_count_);
  system_data[IntermediateDumpKey::kStandardOffsetSeconds].GetInt(
      &standard_offset_seconds_);
  system_data[IntermediateDumpKey::kDaylightOffsetSeconds].GetInt(
      &daylight_offset_seconds_);

  bool has_daylight_saving_time;
  system_data[IntermediateDumpKey::kHasDaylightSavingTime].GetBool(
      &has_daylight_saving_time);
  bool is_daylight_saving_time;
  system_data[IntermediateDumpKey::kIsDaylightSavingTime].GetBool(
      &is_daylight_saving_time);

  if (has_daylight_saving_time) {
    dst_status_ = is_daylight_saving_time
                      ? SystemSnapshot::kObservingDaylightSavingTime
                      : SystemSnapshot::kObservingStandardTime;
  } else {
    dst_status_ = SystemSnapshot::kDoesNotObserveDaylightSavingTime;
  }

  vm_size_t page_size;
  if (system_data[IntermediateDumpKey::kPageSize].AsData().GetValue<vm_size_t>(
          &page_size)) {
    system_data[IntermediateDumpKey::kVMStat][IntermediateDumpKey::kActive]
        .AsData()
        .GetValue<uint64_t>(&active_);
    active_ *= page_size;
    system_data[IntermediateDumpKey::kVMStat][IntermediateDumpKey::kInactive]
        .AsData()
        .GetValue<uint64_t>(&inactive_);
    inactive_ *= page_size;
    system_data[IntermediateDumpKey::kVMStat][IntermediateDumpKey::kWired]
        .AsData()
        .GetValue<uint64_t>(&wired_);
    wired_ *= page_size;
    system_data[IntermediateDumpKey::kVMStat][IntermediateDumpKey::kFree]
        .AsData()
        .GetValue<uint64_t>(&free_);
    free_ *= page_size;
  }
  INITIALIZATION_STATE_SET_VALID(initialized_);
}

CPUArchitecture SystemSnapshotIOSIntermediatedump::GetCPUArchitecture() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
#if defined(ARCH_CPU_X86_64)
  return kCPUArchitectureX86_64;
#elif defined(ARCH_CPU_ARM64)
  return kCPUArchitectureARM64;
#endif
}

uint32_t SystemSnapshotIOSIntermediatedump::CPURevision() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // TODO(justincohen): sysctlbyname machdep.cpu.* returns -1 on iOS/ARM64, but
  // consider recording this for X86_64 only.
  return 0;
}

uint8_t SystemSnapshotIOSIntermediatedump::CPUCount() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return cpu_count_;
}

std::string SystemSnapshotIOSIntermediatedump::CPUVendor() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return cpu_vendor_;
}

void SystemSnapshotIOSIntermediatedump::CPUFrequency(uint64_t* current_hz,
                                                     uint64_t* max_hz) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // TODO(justincohen): sysctlbyname hw.cpufrequency returns -1 on iOS/ARM64,
  // but consider recording this for X86_64 only.
  *current_hz = 0;
  *max_hz = 0;
}

uint32_t SystemSnapshotIOSIntermediatedump::CPUX86Signature() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // TODO(justincohen): Consider recording this for X86_64 only.
  return 0;
}

uint64_t SystemSnapshotIOSIntermediatedump::CPUX86Features() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // TODO(justincohen): Consider recording this for X86_64 only.
  return 0;
}

uint64_t SystemSnapshotIOSIntermediatedump::CPUX86ExtendedFeatures() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // TODO(justincohen): Consider recording this for X86_64 only.
  return 0;
}

uint32_t SystemSnapshotIOSIntermediatedump::CPUX86Leaf7Features() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // TODO(justincohen): Consider recording this for X86_64 only.
  return 0;
}

bool SystemSnapshotIOSIntermediatedump::CPUX86SupportsDAZ() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // TODO(justincohen): Consider recording this for X86_64 only.
  return false;
}

SystemSnapshot::OperatingSystem
SystemSnapshotIOSIntermediatedump::GetOperatingSystem() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return kOperatingSystemIOS;
}

bool SystemSnapshotIOSIntermediatedump::OSServer() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return false;
}

void SystemSnapshotIOSIntermediatedump::OSVersion(int* major,
                                                  int* minor,
                                                  int* bugfix,
                                                  std::string* build) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *major = os_version_major_;
  *minor = os_version_minor_;
  *bugfix = os_version_bugfix_;
  build->assign(os_version_build_);
}

std::string SystemSnapshotIOSIntermediatedump::OSVersionFull() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return base::StringPrintf("%d.%d.%d %s",
                            os_version_major_,
                            os_version_minor_,
                            os_version_bugfix_,
                            os_version_build_.c_str());
}

std::string SystemSnapshotIOSIntermediatedump::MachineDescription() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return machine_description_;
}

bool SystemSnapshotIOSIntermediatedump::NXEnabled() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // TODO(justincohen): Consider using kern.nx when available (pre-iOS 13,
  // pre-OS X 10.15). Otherwise the bit is always enabled.
  return true;
}

void SystemSnapshotIOSIntermediatedump::TimeZone(
    DaylightSavingTimeStatus* dst_status,
    int* standard_offset_seconds,
    int* daylight_offset_seconds,
    std::string* standard_name,
    std::string* daylight_name) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *dst_status = dst_status_;
  *standard_offset_seconds = standard_offset_seconds_;
  *daylight_offset_seconds = daylight_offset_seconds_;
  standard_name->assign(standard_name_);
  daylight_name->assign(daylight_name_);
}

}  // namespace internal
}  // namespace crashpad
