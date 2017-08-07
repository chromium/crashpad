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

#include "snapshot/linux/process_snapshot_linux.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "util/linux/registration_protocol.h"

namespace crashpad {

ProcessSnapshotLinux::ProcessSnapshotLinux()
    : ProcessSnapshot(),
      system_(),
      threads_(),
      process_reader_(),
      report_id_(),
      client_id_(),
      annotations_simple_map_(),
      snapshot_time_(),
      initialized_() {
}

ProcessSnapshotLinux::~ProcessSnapshotLinux() {
}

bool ProcessSnapshotLinux::Initialize(pid_t pid) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  if (gettimeofday(&snapshot_time_, nullptr) != 0) {
    PLOG(ERROR) << "gettimeofday";
    return false;
  }

  if (!process_reader_.Initialize(pid)) {
    return false;
  }

  system_.Initialize(&process_reader, &snapshot_time_);

  InitializeThreads();

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

bool ProcessSnapshotLinux::InitializeException(LinuxVMAddress exception_info_address) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  DCHECK(!exception_);

  ExceptionInformation info;
  if (!process_reader_.Memory()->Read(exception_info_address, sizeof(info), &info)) {
    LOG(ERROR) << "Couldn't read exception info";
    return false;
  }

  exception_.reset(new internal::ExceptionSnapshotLinux());
  if (!exception_->Initialize(&process_reader_,
                              info.siginfo_address,
                              info.context_address,
                              info.thread_id)) {
    exception_.reset();
    return false;
  }

  return true;
}

pid_t ProcessSnapshotLinux::ProcessID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return process_reader_.ProcessID();
}

pid_t ProcessSnapshotLinux::ParentProcessID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return process_reader_.ParentProcessID();
}

void ProcessSnapshotLinux::SnapshotTime(timeval* snapshot_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *snapshot_time = snapshot_time_;
}

void ProcessSnapshotLinux::ProcessStartTime(timeval* start_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  process_reader_.StartTime(start_time);
}

void ProcessSnapshotLinux::ProcessCPUTimes(timeval* user_time,
                                         timeval* system_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  process_reader_.CPUTimes(user_time, system_time);
}

void ProcessSnapshotLinux::ReportID(UUID* report_id) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *report_id = report_id_;
}

void ProcessSnapshotLinux::ClientID(UUID* client_id) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *client_id = client_id_;
}

const std::map<std::string, std::string>&
ProcessSnapshotLinux::AnnotationsSimpleMap() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return annotations_simple_map_;
}

const SystemSnapshot* ProcessSnapshotLinux::System() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &system_;
}

std::vector<const ThreadSnapshot*> ProcessSnapshotLinux::Threads() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  std::vector<const ThreadSnapshot*> threads;
  for (internal::ThreadSnapshotLinux* thread : threads_) {
    threads.push_back(thread);
  }
  return threads;
}

std::vector<const ModuleSnapshot*> ProcessSnapshotLinux::Modules() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // TODO(jperaza): implement module snapshots
  return std::vector<const ModuleSnapshot*>();
}

std::vector<UnloadedModuleSnapshot> ProcessSnapshotLinux::UnloadedModules()
    const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<UnloadedModuleSnapshot>();
}

const ExceptionSnapshot* ProcessSnapshotLinux::Exception() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return exception_.get();
}

std::vector<const MemoryMapRegionSnapshot*> ProcessSnapshotLinux::MemoryMap()
    const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<const MemoryMapRegionSnapshot*>();
}

std::vector<HandleSnapshot> ProcessSnapshotLinux::Handles() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<HandleSnapshot>();
}

std::vector<const MemorySnapshot*> ProcessSnapshotLinux::ExtraMemory() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<const MemorySnapshot*>();
}

void ProcessSnapshotLinux::InitializeThreads() {
  const std::vector<ProcessReader::Thread>& process_reader_threads =
      process_reader_.Threads();
  for (const ProcessReader::Thread& process_reader_thread :
       process_reader_threads) {
    auto thread = base::WrapUnique(new internal::ThreadSnapshotLinux());
    if (thread->Initialize(&process_reader_, process_reader_thread)) {
      threads_.push_back(thread.release());
    }
  }
}

}  // namespace crashpad
