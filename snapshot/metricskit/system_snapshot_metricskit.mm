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

#include "snapshot/metricskit/system_snapshot_metricskit.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "snapshot/cpu_context.h"
#include "snapshot/posix/timezone.h"
#include "util/mac/mac_util.h"
#include "util/numeric/in_range_cast.h"

namespace crashpad {

namespace internal {

SystemSnapshotMetricsKit::SystemSnapshotMetricsKit()
    : cpu_architecture_(kCPUArchitectureUnknown),
      cpu_revision_(0),
      cpu_count_(0),
      cpu_vendor_(),
      cpu_frequency_current_hz_(0),
      cpu_frequency_max_hz_(0),
      cpu_x86_signature_(0),
      cpu_x86_features_(0),
      cpu_x86_extended_features_(0),
      cpu_x86_leaf_7_features_(0),
      cpu_x86_supports_daz_(false),
      operating_system_(kOperatingSystemUnknown),
      os_server_(false),
      os_version_major_(0),
      os_version_minor_(0),
      os_version_bugfix_(0),
      os_version_build_(),
      os_version_full_(),
      nx_enabled_(false),
      machine_description_(),
      time_zone_dst_status_(kDoesNotObserveDaylightSavingTime),
      time_zone_standard_offset_seconds_(0),
      time_zone_daylight_offset_seconds_(0),
      time_zone_standard_name_(),
      time_zone_daylight_name_() {
}

SystemSnapshotMetricsKit::~SystemSnapshotMetricsKit() {
}

void SystemSnapshotMetricsKit::Initialize(NSDictionary* report) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  report_ = report;

  NSDictionary* metadata = base::ObjCCastStrict<NSDictionary>(report[@"diagnosticMetadata"]);

  NSString* arch = base::ObjCCastStrict<NSString>(report[@"platformArchitecture"]);
  if ([arch isEqualToString:@"arm64"]) {
    cpu_architecture_ = kCPUArchitectureARM64;
  }

  NSString os = base::ObjCCastStrict<NSString>(report[@"osVersion"]);
  operating_system_ = kOperatingSystemIOS;
  
  INITIALIZATION_STATE_SET_VALID(initialized_);
}
CPUArchitecture SystemSnapshotMetricsKit::GetCPUArchitecture() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return cpu_architecture_;
}

uint32_t SystemSnapshotMetricsKit::CPURevision() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return cpu_revision_;
}

uint8_t SystemSnapshotMetricsKit::CPUCount() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return cpu_count_;
}

std::string SystemSnapshotMetricsKit::CPUVendor() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return cpu_vendor_;
}

void SystemSnapshotMetricsKit::CPUFrequency(uint64_t* current_hz,
                                      uint64_t* max_hz) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *current_hz = cpu_frequency_current_hz_;
  *max_hz = cpu_frequency_max_hz_;
}

uint32_t SystemSnapshotMetricsKit::CPUX86Signature() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return cpu_x86_signature_;
}

uint64_t SystemSnapshotMetricsKit::CPUX86Features() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return cpu_x86_features_;
}

uint64_t SystemSnapshotMetricsKit::CPUX86ExtendedFeatures() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return cpu_x86_extended_features_;
}

uint32_t SystemSnapshotMetricsKit::CPUX86Leaf7Features() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return cpu_x86_leaf_7_features_;
}

bool SystemSnapshotMetricsKit::CPUX86SupportsDAZ() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return cpu_x86_supports_daz_;
}

SystemSnapshot::OperatingSystem SystemSnapshotMetricsKit::GetOperatingSystem() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return operating_system_;
}

bool SystemSnapshotMetricsKit::OSServer() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return os_server_;
}

void SystemSnapshotMetricsKit::OSVersion(
    int* major, int* minor, int* bugfix, std::string* build) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *major = os_version_major_;
  *minor = os_version_minor_;
  *bugfix = os_version_bugfix_;
  *build = os_version_build_;
}

std::string SystemSnapshotMetricsKit::OSVersionFull() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return os_version_full_;
}

bool SystemSnapshotMetricsKit::NXEnabled() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return nx_enabled_;
}

std::string SystemSnapshotMetricsKit::MachineDescription() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return machine_description_;
}

void SystemSnapshotMetricsKit::TimeZone(DaylightSavingTimeStatus* dst_status,
                                  int* standard_offset_seconds,
                                  int* daylight_offset_seconds,
                                  std::string* standard_name,
                                  std::string* daylight_name) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *dst_status = time_zone_dst_status_;
  *standard_offset_seconds = time_zone_standard_offset_seconds_;
  *daylight_offset_seconds = time_zone_daylight_offset_seconds_;
  *standard_name = time_zone_standard_name_;
  *daylight_name = time_zone_daylight_name_;
}

}  // namespace internal
}  // namespace crashpad
