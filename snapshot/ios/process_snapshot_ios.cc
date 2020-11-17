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

#include <sys/stat.h>

#include "base/logging.h"
#include "util/ios/pack_ios_state.h"

namespace {

void MachTimeValueToTimeval(const time_value& mach, timeval* tv) {
  tv->tv_sec = mach.seconds;
  tv->tv_usec = mach.microseconds;
}

}  // namespace

namespace crashpad {

ProcessSnapshotIOS::ProcessSnapshotIOS()
    : ProcessSnapshot(),
      mapping_(),
      basic_info_user_time_(),
      basic_info_system_time_(),
      thread_times_user_time_(),
      thread_times_system_time_(),
      system_(),
      threads_(),
      modules_(),
      exception_(),
      report_id_(),
      client_id_(),
      annotations_simple_map_(),
      snapshot_time_(),
      initialized_() {}

bool ProcessSnapshotIOS::Initialize(
    const base::FilePath& dump_path,
    const std::map<std::string, std::string>& annotations) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  annotations_simple_map_ = annotations;
  fd_ = base::ScopedFD(open(dump_path.value().c_str(), O_RDONLY));
  if (!fd_.is_valid())
    return false;

  struct stat filestat;
  fstat(fd_.get(), &filestat);
  if (filestat.st_size == 0) {
    return false;
  }
  mapping_.ResetMmap(
      NULL, filestat.st_size, PROT_READ, MAP_SHARED, fd_.get(), 0);

  PackedMap in_process_dump =
      PackedMap::Parse(mapping_.addr_as<uint8_t*>(), mapping_.len());
  in_process_dump.DebugDump();

  uint8_t version;
  in_process_dump["version"].AsData().GetData<uint8_t>(&version);
  if (version != 1)
    return false;

  const PackedMap& process_info = in_process_dump["ProcessInfo"].AsMap();
  process_info["kern_proc_info"]["p_pid"].AsData().GetData<pid_t>(&p_pid_);
  process_info["kern_proc_info"]["e_ppid"].AsData().GetData<pid_t>(&e_ppid_);
  process_info["kern_proc_info"]["p_starttime"].AsData().GetData<timeval>(
      &p_starttime_);
  process_info["task_basic_info"]["user_time"].AsData().GetData<time_value_t>(
      &basic_info_user_time_);
  process_info["task_basic_info"]["system_time"].AsData().GetData<time_value_t>(
      &basic_info_system_time_);
  process_info["task_thread_times"]["user_time"].AsData().GetData<time_value_t>(
      &thread_times_user_time_);
  process_info["task_thread_times"]["system_time"]
      .AsData()
      .GetData<time_value_t>(&thread_times_system_time_);
  process_info["snapshot_time"].AsData().GetData<timeval>(&snapshot_time_);

  system_.Initialize(in_process_dump["SystemInfo"].AsMap());

  // Threads
  const auto& thread_list = in_process_dump["Threads"].AsList();
  for (auto& value : thread_list) {
    auto thread = std::make_unique<internal::ThreadSnapshotIOS>();
    if (thread->Initialize((*value).AsMap())) {
      threads_.push_back(std::move(thread));
    }
  }

  const auto& module_list = in_process_dump["Modules"].AsList();
  for (auto& value : module_list) {
    auto module = std::make_unique<internal::ModuleSnapshotIOS>();
    if (module->Initialize((*value).AsMap())) {
      modules_.push_back(std::move(module));
    }
  }

  // Exceptions
  if (in_process_dump.HasKey("SignalException")) {
    exception_.reset(new internal::ExceptionSnapshotIOS());
    exception_->InitializeFromSignal(
        in_process_dump["SignalException"].AsMap());
  } else if (in_process_dump.HasKey("MachException")) {
    exception_.reset(new internal::ExceptionSnapshotIOS());
    exception_->InitializeFromMachException(
        in_process_dump["MachException"].AsMap(),
        in_process_dump["Threads"].AsList());
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

pid_t ProcessSnapshotIOS::ProcessID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return p_pid_;
}

pid_t ProcessSnapshotIOS::ParentProcessID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return e_ppid_;
}

void ProcessSnapshotIOS::SnapshotTime(timeval* snapshot_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *snapshot_time = snapshot_time_;
}

void ProcessSnapshotIOS::ProcessStartTime(timeval* start_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *start_time = p_starttime_;
}

void ProcessSnapshotIOS::ProcessCPUTimes(timeval* user_time,
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
  return &system_;
}

std::vector<const ThreadSnapshot*> ProcessSnapshotIOS::Threads() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  std::vector<const ThreadSnapshot*> threads;
  for (const auto& thread : threads_) {
    threads.push_back(thread.get());
  }
  return threads;
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
  return exception_.get();
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

}  // namespace crashpad
