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

#ifndef CRASHPAD_SNAPSHOT_METRICSKIT_SYSTEM_SNAPSHOT_METRICSKIT_H_
#define CRASHPAD_SNAPSHOT_METRICSKIT_SYSTEM_SNAPSHOT_METRICSKIT_H_

#include <Foundation/Foundation.h>

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "snapshot/system_snapshot.h"
#include "util/misc/initialization_state_dcheck.h"

namespace crashpad {

namespace internal {

//! \brief A SystemSnapshot of the running system, when the system runs iOS.
class SystemSnapshotMetricsKit final : public SystemSnapshot {
 public:
  SystemSnapshotMetricsKit();
  ~SystemSnapshotMetricsKit() override;

  void Initialize(NSDictionary* report);

  // SystemSnapshot:
  CPUArchitecture GetCPUArchitecture() const override;
  uint32_t CPURevision() const override;
  uint8_t CPUCount() const override;
  std::string CPUVendor() const override;
  void CPUFrequency(uint64_t* current_hz, uint64_t* max_hz) const override;
  uint32_t CPUX86Signature() const override;
  uint64_t CPUX86Features() const override;
  uint64_t CPUX86ExtendedFeatures() const override;
  uint32_t CPUX86Leaf7Features() const override;
  bool CPUX86SupportsDAZ() const override;
  OperatingSystem GetOperatingSystem() const override;
  bool OSServer() const override;
  void OSVersion(
      int* major, int* minor, int* bugfix, std::string* build) const override;
  std::string OSVersionFull() const override;
  bool NXEnabled() const override;
  std::string MachineDescription() const override;
  void TimeZone(DaylightSavingTimeStatus* dst_status,
                int* standard_offset_seconds,
                int* daylight_offset_seconds,
                std::string* standard_name,
                std::string* daylight_name) const override;

 private:
  __strong NSDictionary* report_;
  CPUArchitecture cpu_architecture_;
  uint32_t cpu_revision_;
  uint8_t cpu_count_;
  std::string cpu_vendor_;
  uint64_t cpu_frequency_current_hz_;
  uint64_t cpu_frequency_max_hz_;
  uint32_t cpu_x86_signature_;
  uint64_t cpu_x86_features_;
  uint64_t cpu_x86_extended_features_;
  uint32_t cpu_x86_leaf_7_features_;
  bool cpu_x86_supports_daz_;
  OperatingSystem operating_system_;
  bool os_server_;
  int os_version_major_;
  int os_version_minor_;
  int os_version_bugfix_;
  std::string os_version_build_;
  std::string os_version_full_;
  bool nx_enabled_;
  std::string machine_description_;
  DaylightSavingTimeStatus time_zone_dst_status_;
  int time_zone_standard_offset_seconds_;
  int time_zone_daylight_offset_seconds_;
  std::string time_zone_standard_name_;
  std::string time_zone_daylight_name_;

  InitializationStateDcheck initialized_;


  DISALLOW_COPY_AND_ASSIGN(SystemSnapshotMetricsKit);
};

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_METRICSKIT_SYSTEM_SNAPSHOT_METRICSKIT_H_
