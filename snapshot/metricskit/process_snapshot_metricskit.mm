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

#include "snapshot/metricskit/process_snapshot_metricskit.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "snapshot/metricskit/system_snapshot_metricskit.h"

namespace crashpad {

ProcessSnapshotMetricsKit::ProcessSnapshotMetricsKit()
    : ProcessSnapshot(),
      system_(),
      report_id_(),
      client_id_(),
      annotations_simple_map_(),
      snapshot_time_(),
      initialized_() {}

ProcessSnapshotMetricsKit::~ProcessSnapshotMetricsKit() {}

bool ProcessSnapshotMetricsKit::Initialize(NSDictionary* report) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  system_.Initialize(report);
  
  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

pid_t ProcessSnapshotMetricsKit::ProcessID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return 8888;
}

pid_t ProcessSnapshotMetricsKit::ParentProcessID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return 8887;
}

void ProcessSnapshotMetricsKit::SnapshotTime(timeval* snapshot_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  if (!gettimeofday(snapshot_time, nullptr)) {
    timerclear(snapshot_time);
  }
}

void ProcessSnapshotMetricsKit::ProcessStartTime(timeval* start_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // TODO(rohitrao): Figure this out.
}

void ProcessSnapshotMetricsKit::ProcessCPUTimes(timeval* user_time,
                                                timeval* system_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  timerclear(user_time);
  timerclear(system_time);

  // TODO(rohitrao): Figure this out.
}

void ProcessSnapshotMetricsKit::ReportID(UUID* report_id) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // TODO(rohitrao): Figure this out.
  *report_id = report_id_;
}

void ProcessSnapshotMetricsKit::ClientID(UUID* client_id) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // TODO(rohitrao): Figure this out.
  *client_id = client_id_;
}

const std::map<std::string, std::string>&
ProcessSnapshotMetricsKit::AnnotationsSimpleMap() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return annotations_simple_map_;
}

const SystemSnapshot* ProcessSnapshotMetricsKit::System() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &system_;
}

std::vector<const ThreadSnapshot*> ProcessSnapshotMetricsKit::Threads() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  std::vector<const ThreadSnapshot*> threads;
  return threads;

  // TODO(rohitrao): Figure this out.
  // for (const auto& thread : threads_) {
  //   threads.push_back(thread.get());
  // }
}

std::vector<const ModuleSnapshot*> ProcessSnapshotMetricsKit::Modules() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  std::vector<const ModuleSnapshot*> modules;
  return modules;

  // TODO(rohitrao): Figure this out.
  // for (const auto& module : modules_) {
  //   modules.push_back(module.get());
  // }
}

std::vector<UnloadedModuleSnapshot> ProcessSnapshotMetricsKit::UnloadedModules()
    const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<UnloadedModuleSnapshot>();
}

const ExceptionSnapshot* ProcessSnapshotMetricsKit::Exception() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return nullptr;

  // TODO(rohitrao): Figure this out.
  // return exception_.get();
}

std::vector<const MemoryMapRegionSnapshot*>
ProcessSnapshotMetricsKit::MemoryMap() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<const MemoryMapRegionSnapshot*>();
}

std::vector<HandleSnapshot> ProcessSnapshotMetricsKit::Handles() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<HandleSnapshot>();
}

std::vector<const MemorySnapshot*> ProcessSnapshotMetricsKit::ExtraMemory()
    const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<const MemorySnapshot*>();
}

const ProcessMemory* ProcessSnapshotMetricsKit::Memory() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return nullptr;
}

}  // namespace crashpad
