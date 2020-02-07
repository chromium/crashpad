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

#include <utility>

#include "base/logging.h"

namespace crashpad {

ProcessSnapshotIOS::ProcessSnapshotIOS()
    : ProcessSnapshot(),
//      system_(),
      // threads_(),
      modules_(),
      // exception_(),
      process_reader_(),
      report_id_(),
      client_id_(),
      annotations_simple_map_(),
      snapshot_time_(),
      initialized_() {
}

ProcessSnapshotIOS::~ProcessSnapshotIOS() {
}

bool ProcessSnapshotIOS::Initialize() {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  if (gettimeofday(&snapshot_time_, nullptr) != 0) {
    PLOG(ERROR) << "gettimeofday";
    return false;
  }

  if (!process_reader_.Initialize()) {
    return false;
  }

//  system_.Initialize(&process_reader_, &snapshot_time_);

  // InitializeThreads();
  InitializeModules();

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}


pid_t ProcessSnapshotIOS::ProcessID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return process_reader_.ProcessID();
}

pid_t ProcessSnapshotIOS::ParentProcessID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return process_reader_.ParentProcessID();
}

void ProcessSnapshotIOS::SnapshotTime(timeval* snapshot_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *snapshot_time = snapshot_time_;
}

void ProcessSnapshotIOS::ProcessStartTime(timeval* start_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  process_reader_.StartTime(start_time);
}

void ProcessSnapshotIOS::ProcessCPUTimes(timeval* user_time,
                                         timeval* system_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  process_reader_.CPUTimes(user_time, system_time);
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
//  return &system_;
  return nullptr;
}

std::vector<const ThreadSnapshot*> ProcessSnapshotIOS::Threads() const {
  // INITIALIZATION_STATE_DCHECK_VALID(initialized_);
   std::vector<const ThreadSnapshot*> threads;
  // for (const auto& thread : threads_) {
  //   threads.push_back(thread.get());
  // }
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
//  return exception_.get();
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
//  return process_reader_.Memory();
  return nullptr;
}

void ProcessSnapshotIOS::InitializeThreads() {
  // const std::vector<ProcessReaderIOS::Thread>& process_reader_threads =
  //     process_reader_.Threads();
  // for (const ProcessReaderIOS::Thread& process_reader_thread :
  //      process_reader_threads) {
  //   auto thread = std::make_unique<internal::ThreadSnapshotIOS>();
  //   if (thread->Initialize(&process_reader_, process_reader_thread)) {
  //     threads_.push_back(std::move(thread));
  //   }
  // }
}

void ProcessSnapshotIOS::InitializeModules() {
  const std::vector<ProcessReaderIOS::Module>& process_reader_modules =
      process_reader_.Modules();
  for (const ProcessReaderIOS::Module& process_reader_module :
       process_reader_modules) {
    auto module = std::make_unique<internal::ModuleSnapshotIOS>();
    if (module->Initialize(&process_reader_, process_reader_module)) {
      modules_.push_back(std::move(module));
    }
  }
}

}  // namespace crashpad
