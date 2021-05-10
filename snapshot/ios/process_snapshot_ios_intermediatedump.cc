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

#include "snapshot/ios/process_snapshot_ios_intermediatedump.h"

#include <sys/stat.h>

#include "base/logging.h"
#include "util/ios/ios_intermediatedump_data.h"
#include "util/ios/ios_intermediatedump_list.h"
#include "util/ios/ios_intermediatedump_map.h"

namespace {

void MachTimeValueToTimeval(const time_value& mach, timeval* tv) {
  tv->tv_sec = mach.seconds;
  tv->tv_usec = mach.microseconds;
}

}  // namespace

namespace crashpad {

using internal::IntermediateDumpKey;

bool ProcessSnapshotIOSIntermediatedump::Initialize(
    const base::FilePath& dump_path,
    const std::map<std::string, std::string>& annotations) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  annotations_simple_map_ = annotations;

  if (!reader_.Initialize(dump_path)) {
    return false;
  }

  if (!reader_.Parse()) {
    DLOG(ERROR) << "Parsing failed, attempting to load partial dump.";
  }

  auto& root_map = reader_.RootMap();

  uint8_t version;
  root_map[IntermediateDumpKey::kVersion].AsData().GetValue<uint8_t>(&version);
  if (version != 1)
    return false;

  const internal::IOSIntermediatedumpMap& process_info =
      root_map[IntermediateDumpKey::kProcessInfo].AsMap();
  process_info[IntermediateDumpKey::kPID].AsData().GetValue<pid_t>(&p_pid_);
  process_info[IntermediateDumpKey::kParentPID].AsData().GetValue<pid_t>(
      &e_ppid_);
  process_info[IntermediateDumpKey::kStartTime].AsData().GetValue<timeval>(
      &p_starttime_);
  process_info[IntermediateDumpKey::kTaskBasicInfo]
              [IntermediateDumpKey::kUserTime]
                  .AsData()
                  .GetValue<time_value_t>(&basic_info_user_time_);
  process_info[IntermediateDumpKey::kTaskBasicInfo]
              [IntermediateDumpKey::kSystemTime]
                  .AsData()
                  .GetValue<time_value_t>(&basic_info_system_time_);
  process_info[IntermediateDumpKey::kTaskThreadTimes]
              [IntermediateDumpKey::kUserTime]
                  .AsData()
                  .GetValue<time_value_t>(&thread_times_user_time_);
  process_info[IntermediateDumpKey::kTaskThreadTimes]
              [IntermediateDumpKey::kSystemTime]
                  .AsData()
                  .GetValue<time_value_t>(&thread_times_system_time_);
  process_info[IntermediateDumpKey::kSnapshotTime].AsData().GetValue<timeval>(
      &snapshot_time_);

  system_.Initialize(root_map[IntermediateDumpKey::kSystemInfo].AsMap());

  // Threads
  const auto& thread_list = root_map[IntermediateDumpKey::kThreads].AsList();
  for (auto& value : thread_list) {
    auto thread =
        std::make_unique<internal::ThreadSnapshotIOSIntermediatedump>();
    if (thread->Initialize((*value).AsMap())) {
      threads_.push_back(std::move(thread));
    }
  }

  const auto& module_list = root_map[IntermediateDumpKey::kModules].AsList();
  for (auto& value : module_list) {
    auto module =
        std::make_unique<internal::ModuleSnapshotIOSIntermediatedump>();
    if (module->Initialize((*value).AsMap())) {
      modules_.push_back(std::move(module));
    }
  }

  // Exceptions
  if (root_map.HasKey(IntermediateDumpKey::kSignalException)) {
    exception_.reset(new internal::ExceptionSnapshotIOSIntermediatedump());
    exception_->InitializeFromSignal(
        root_map[IntermediateDumpKey::kSignalException].AsMap());
  } else if (root_map.HasKey(IntermediateDumpKey::kMachException)) {
    exception_.reset(new internal::ExceptionSnapshotIOSIntermediatedump());
    exception_->InitializeFromMachException(
        root_map[IntermediateDumpKey::kMachException].AsMap(),
        root_map[IntermediateDumpKey::kThreads].AsList());
  } else if (root_map.HasKey(IntermediateDumpKey::kNSException)) {
    exception_.reset(new internal::ExceptionSnapshotIOSIntermediatedump());
    exception_->InitializeFromNSException(
        root_map[IntermediateDumpKey::kNSException].AsMap(),
        root_map[IntermediateDumpKey::kThreads].AsList());
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

pid_t ProcessSnapshotIOSIntermediatedump::ProcessID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return p_pid_;
}

pid_t ProcessSnapshotIOSIntermediatedump::ParentProcessID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return e_ppid_;
}

void ProcessSnapshotIOSIntermediatedump::SnapshotTime(
    timeval* snapshot_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *snapshot_time = snapshot_time_;
}

void ProcessSnapshotIOSIntermediatedump::ProcessStartTime(
    timeval* start_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *start_time = p_starttime_;
}

void ProcessSnapshotIOSIntermediatedump::ProcessCPUTimes(
    timeval* user_time,
    timeval* system_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  // Calculate user and system time the same way the kernel does for
  // getrusage(). See 10.15.0 xnu-6153.11.26/bsd/kern/kern_resource.c calcru().
  timerclear(user_time);
  timerclear(system_time);

  MachTimeValueToTimeval(basic_info_user_time_, user_time);
  MachTimeValueToTimeval(basic_info_system_time_, system_time);

  timeval thread_user_time;
  MachTimeValueToTimeval(thread_times_user_time_, &thread_user_time);
  timeval thread_system_time;
  MachTimeValueToTimeval(thread_times_system_time_, &thread_system_time);

  timeradd(user_time, &thread_user_time, user_time);
  timeradd(system_time, &thread_system_time, system_time);
}

void ProcessSnapshotIOSIntermediatedump::ReportID(UUID* report_id) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *report_id = report_id_;
}

void ProcessSnapshotIOSIntermediatedump::ClientID(UUID* client_id) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *client_id = client_id_;
}

const std::map<std::string, std::string>&
ProcessSnapshotIOSIntermediatedump::AnnotationsSimpleMap() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return annotations_simple_map_;
}

const SystemSnapshot* ProcessSnapshotIOSIntermediatedump::System() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &system_;
}

std::vector<const ThreadSnapshot*> ProcessSnapshotIOSIntermediatedump::Threads()
    const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  std::vector<const ThreadSnapshot*> threads;
  for (const auto& thread : threads_) {
    threads.push_back(thread.get());
  }
  return threads;
}

std::vector<const ModuleSnapshot*> ProcessSnapshotIOSIntermediatedump::Modules()
    const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  std::vector<const ModuleSnapshot*> modules;
  for (const auto& module : modules_) {
    modules.push_back(module.get());
  }
  return modules;
}

std::vector<UnloadedModuleSnapshot>
ProcessSnapshotIOSIntermediatedump::UnloadedModules() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<UnloadedModuleSnapshot>();
}

const ExceptionSnapshot* ProcessSnapshotIOSIntermediatedump::Exception() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return exception_.get();
}

std::vector<const MemoryMapRegionSnapshot*>
ProcessSnapshotIOSIntermediatedump::MemoryMap() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<const MemoryMapRegionSnapshot*>();
}

std::vector<HandleSnapshot> ProcessSnapshotIOSIntermediatedump::Handles()
    const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<HandleSnapshot>();
}

std::vector<const MemorySnapshot*>
ProcessSnapshotIOSIntermediatedump::ExtraMemory() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<const MemorySnapshot*>();
}

const ProcessMemory* ProcessSnapshotIOSIntermediatedump::Memory() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return nullptr;
}

}  // namespace crashpad
