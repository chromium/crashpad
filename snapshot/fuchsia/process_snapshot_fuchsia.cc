// Copyright 2017 The Crashpad Authors. All rights reserved.
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

#include "snapshot/fuchsia/process_snapshot_fuchsia.h"

#include "base/logging.h"

namespace crashpad {

ProcessSnapshotFuchsia::ProcessSnapshotFuchsia() {}

ProcessSnapshotFuchsia::~ProcessSnapshotFuchsia() {}

bool ProcessSnapshotFuchsia::Initialize(zx_handle_t process) {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
  return false;
}

void ProcessSnapshotFuchsia::GetCrashpadOptions(
    CrashpadInfoClientOptions* options) {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
}

pid_t ProcessSnapshotFuchsia::ProcessID() const {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
  return 0;
}

pid_t ProcessSnapshotFuchsia::ParentProcessID() const {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
  return 0;
}

void ProcessSnapshotFuchsia::SnapshotTime(timeval* snapshot_time) const {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
}

void ProcessSnapshotFuchsia::ProcessStartTime(timeval* start_time) const {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
}

void ProcessSnapshotFuchsia::ProcessCPUTimes(timeval* user_time,
                                             timeval* system_time) const {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
}

void ProcessSnapshotFuchsia::ReportID(UUID* report_id) const {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
}

void ProcessSnapshotFuchsia::ClientID(UUID* client_id) const {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
}

const std::map<std::string, std::string>&
ProcessSnapshotFuchsia::AnnotationsSimpleMap() const {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
  return annotations_simple_map_;
}

const SystemSnapshot* ProcessSnapshotFuchsia::System() const {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
  return nullptr;
}

std::vector<const ThreadSnapshot*> ProcessSnapshotFuchsia::Threads() const {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
  return std::vector<const ThreadSnapshot*>();
}

std::vector<const ModuleSnapshot*> ProcessSnapshotFuchsia::Modules() const {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
  return std::vector<const ModuleSnapshot*>();
}

std::vector<UnloadedModuleSnapshot> ProcessSnapshotFuchsia::UnloadedModules()
    const {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
  return std::vector<UnloadedModuleSnapshot>();
}

const ExceptionSnapshot* ProcessSnapshotFuchsia::Exception() const {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
  return nullptr;
}

std::vector<const MemoryMapRegionSnapshot*> ProcessSnapshotFuchsia::MemoryMap()
    const {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
  return std::vector<const MemoryMapRegionSnapshot*>();
}

std::vector<HandleSnapshot> ProcessSnapshotFuchsia::Handles() const {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
  return std::vector<HandleSnapshot>();
}

std::vector<const MemorySnapshot*> ProcessSnapshotFuchsia::ExtraMemory() const {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
  return std::vector<const MemorySnapshot*>();
}

}  // namespace crashpad
