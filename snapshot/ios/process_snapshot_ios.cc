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

#include "snapshot/ios/process_snapshot_ios.h"

#include <mach-o/loader.h>
#include <mach/mach.h>
#include <utility>
#include "base/logging.h"
#include "base/mac/mach_logging.h"

namespace crashpad {

ProcessSnapshotIOS::ProcessSnapshotIOS()
    : ProcessSnapshot(),
      modules_(),
      report_id_(),
      client_id_(),
      annotations_simple_map_(),
      snapshot_time_(),
      initialized_() {}

ProcessSnapshotIOS::~ProcessSnapshotIOS() {}

bool ProcessSnapshotIOS::Initialize() {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  if (gettimeofday(&snapshot_time_, nullptr) != 0) {
    PLOG(ERROR) << "gettimeofday";
    return false;
  }

  InitializeModules();

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

pid_t ProcessSnapshotIOS::ProcessID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return getpid();
}

pid_t ProcessSnapshotIOS::ParentProcessID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return 0;
}

void ProcessSnapshotIOS::SnapshotTime(timeval* snapshot_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *snapshot_time = snapshot_time_;
}

void ProcessSnapshotIOS::ProcessStartTime(timeval* start_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
}

void ProcessSnapshotIOS::ProcessCPUTimes(timeval* user_time,
                                         timeval* system_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
}

void ProcessSnapshotIOS::ReportID(UUID* report_id) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *report_id = report_id_;
}

void ProcessSnapshotIOS::ClientID(UUID* client_id) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *client_id = client_id_;
}

const std::map<std::string, std::string>&
ProcessSnapshotIOS::AnnotationsSimpleMap() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return annotations_simple_map_;
}

const SystemSnapshot* ProcessSnapshotIOS::System() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return nullptr;
}

std::vector<const ThreadSnapshot*> ProcessSnapshotIOS::Threads() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<const ThreadSnapshot*>();
}

std::vector<const ModuleSnapshot*> ProcessSnapshotIOS::Modules() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  std::vector<const ModuleSnapshot*> modules;
  for (const auto& module : modules_) {
    modules.push_back(module.get());
  }
  return modules;
}

std::vector<UnloadedModuleSnapshot> ProcessSnapshotIOS::UnloadedModules()
    const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<UnloadedModuleSnapshot>();
}

const ExceptionSnapshot* ProcessSnapshotIOS::Exception() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return nullptr;
}

std::vector<const MemoryMapRegionSnapshot*> ProcessSnapshotIOS::MemoryMap()
    const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<const MemoryMapRegionSnapshot*>();
}

std::vector<HandleSnapshot> ProcessSnapshotIOS::Handles() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<HandleSnapshot>();
}

std::vector<const MemorySnapshot*> ProcessSnapshotIOS::ExtraMemory() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<const MemorySnapshot*>();
}

const ProcessMemory* ProcessSnapshotIOS::Memory() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return nullptr;
}

void ProcessSnapshotIOS::InitializeModules() {
  mach_vm_size_t all_image_info_size;
  mach_vm_address_t all_image_info_addr =
      internal::ModuleSnapshotIOS::DyldAllImageInfo(&all_image_info_size);

  struct dyld_all_image_infos* image_info =
      (struct dyld_all_image_infos*)all_image_info_addr;
  auto module = std::make_unique<internal::ModuleSnapshotIOS>();
  if (module->InitializeDyld(image_info)) {
    modules_.push_back(std::move(module));
  }
  uint32_t image_count = image_info->infoArrayCount;
  const struct dyld_image_info* image_array = image_info->infoArray;
  const struct dyld_image_info* image;
  for (uint32_t i = 0; i < image_count; ++i) {
    image = image_array + i;
    auto module = std::make_unique<internal::ModuleSnapshotIOS>();
    if (module->Initialize(image)) {
      modules_.push_back(std::move(module));
    }
  }
}

}  // namespace crashpad
