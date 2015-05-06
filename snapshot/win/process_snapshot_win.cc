// Copyright 2015 The Crashpad Authors. All rights reserved.
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

#include "snapshot/win/process_snapshot_win.h"

#include "snapshot/win/module_snapshot_win.h"
#include "util/win/time.h"

namespace crashpad {

ProcessSnapshotWin::ProcessSnapshotWin()
    : ProcessSnapshot(),
      system_(),
      // TODO(scottmg): threads_(),
      modules_(),
      // TODO(scottmg): exception_(),
      process_reader_(),
      report_id_(),
      client_id_(),
      annotations_simple_map_(),
      snapshot_time_(),
      initialized_() {
}

ProcessSnapshotWin::~ProcessSnapshotWin() {
}

bool ProcessSnapshotWin::Initialize(HANDLE process) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  GetTimeOfDay(&snapshot_time_);

  if (!process_reader_.Initialize(process))
    return false;

  system_.Initialize(&process_reader_);

  // TODO(scottmg): InitializeThreads();
  InitializeModules();

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

void ProcessSnapshotWin::GetCrashpadOptions(
    CrashpadInfoClientOptions* options) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  CrashpadInfoClientOptions local_options;

  for (internal::ModuleSnapshotWin* module : modules_) {
    CrashpadInfoClientOptions module_options;
    module->GetCrashpadOptions(&module_options);

    if (local_options.crashpad_handler_behavior == TriState::kUnset) {
      local_options.crashpad_handler_behavior =
          module_options.crashpad_handler_behavior;
    }
    if (local_options.system_crash_reporter_forwarding == TriState::kUnset) {
      local_options.system_crash_reporter_forwarding =
          module_options.system_crash_reporter_forwarding;
    }

    // If non-default values have been found for all options, the loop can end
    // early.
    if (local_options.crashpad_handler_behavior != TriState::kUnset &&
        local_options.system_crash_reporter_forwarding != TriState::kUnset) {
      break;
    }
  }

  *options = local_options;
}

pid_t ProcessSnapshotWin::ProcessID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return process_reader_.ProcessID();
}

pid_t ProcessSnapshotWin::ParentProcessID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return process_reader_.ParentProcessID();
}

void ProcessSnapshotWin::SnapshotTime(timeval* snapshot_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *snapshot_time = snapshot_time_;
}

void ProcessSnapshotWin::ProcessStartTime(timeval* start_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  process_reader_.StartTime(start_time);
}

void ProcessSnapshotWin::ProcessCPUTimes(timeval* user_time,
                                         timeval* system_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  process_reader_.CPUTimes(user_time, system_time);
}

void ProcessSnapshotWin::ReportID(UUID* report_id) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *report_id = report_id_;
}

void ProcessSnapshotWin::ClientID(UUID* client_id) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *client_id = client_id_;
}

const std::map<std::string, std::string>&
ProcessSnapshotWin::AnnotationsSimpleMap() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return annotations_simple_map_;
}

const SystemSnapshot* ProcessSnapshotWin::System() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &system_;
}

std::vector<const ThreadSnapshot*> ProcessSnapshotWin::Threads() const {
  CHECK(false) << "TODO(scottmg)";
  return std::vector<const ThreadSnapshot*>();
}

std::vector<const ModuleSnapshot*> ProcessSnapshotWin::Modules() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  std::vector<const ModuleSnapshot*> modules;
  for (internal::ModuleSnapshotWin* module : modules_) {
    modules.push_back(module);
  }
  return modules;
}

const ExceptionSnapshot* ProcessSnapshotWin::Exception() const {
  CHECK(false) << "TODO(scottmg)";
  return nullptr;
}

void ProcessSnapshotWin::InitializeModules() {
  const std::vector<ProcessInfo::Module>& process_reader_modules =
      process_reader_.Modules();
  for (const ProcessInfo::Module& process_reader_module :
       process_reader_modules) {
    auto module = make_scoped_ptr(new internal::ModuleSnapshotWin());
    if (module->Initialize(&process_reader_, process_reader_module)) {
      modules_.push_back(module.release());
    }
  }
}

}  // namespace crashpad
