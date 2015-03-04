// Copyright 2014 The Crashpad Authors. All rights reserved.
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

#include "snapshot/mac/process_snapshot_mac.h"

namespace crashpad {

ProcessSnapshotMac::ProcessSnapshotMac()
    : ProcessSnapshot(),
      system_(),
      threads_(),
      modules_(),
      exception_(),
      process_reader_(),
      annotations_simple_map_(),
      snapshot_time_(),
      initialized_() {
}

ProcessSnapshotMac::~ProcessSnapshotMac() {
}

bool ProcessSnapshotMac::Initialize(task_t task) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  if (gettimeofday(&snapshot_time_, nullptr) != 0) {
    PLOG(ERROR) << "gettimeofday";
    return false;
  }

  if (!process_reader_.Initialize(task)) {
    return false;
  }

  system_.Initialize(&process_reader_, &snapshot_time_);

  InitializeThreads();
  InitializeModules();

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

bool ProcessSnapshotMac::InitializeException(
    thread_t exception_thread,
    exception_type_t exception,
    const mach_exception_data_type_t* code,
    mach_msg_type_number_t code_count,
    thread_state_flavor_t flavor,
    const natural_t* state,
    mach_msg_type_number_t state_count) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  DCHECK(!exception_);

  exception_.reset(new internal::ExceptionSnapshotMac());
  if (!exception_->Initialize(&process_reader_,
                              exception_thread,
                              exception,
                              code,
                              code_count,
                              flavor,
                              state,
                              state_count)) {
    exception_.reset();
    return false;
  }

  return true;
}

pid_t ProcessSnapshotMac::ProcessID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return process_reader_.ProcessID();
}

pid_t ProcessSnapshotMac::ParentProcessID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return process_reader_.ProcessID();
}

void ProcessSnapshotMac::SnapshotTime(timeval* snapshot_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *snapshot_time = snapshot_time_;
}

void ProcessSnapshotMac::ProcessStartTime(timeval* start_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  process_reader_.StartTime(start_time);
}

void ProcessSnapshotMac::ProcessCPUTimes(timeval* user_time,
                                         timeval* system_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  process_reader_.CPUTimes(user_time, system_time);
}

const std::map<std::string, std::string>&
ProcessSnapshotMac::AnnotationsSimpleMap() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return annotations_simple_map_;
}

const SystemSnapshot* ProcessSnapshotMac::System() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &system_;
}

std::vector<const ThreadSnapshot*> ProcessSnapshotMac::Threads() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  std::vector<const ThreadSnapshot*> threads;
  for (internal::ThreadSnapshotMac* thread : threads_) {
    threads.push_back(thread);
  }
  return threads;
}

std::vector<const ModuleSnapshot*> ProcessSnapshotMac::Modules() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  std::vector<const ModuleSnapshot*> modules;
  for (internal::ModuleSnapshotMac* module : modules_) {
    modules.push_back(module);
  }
  return modules;
}

const ExceptionSnapshot* ProcessSnapshotMac::Exception() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return exception_.get();
}

void ProcessSnapshotMac::InitializeThreads() {
  const std::vector<ProcessReader::Thread>& process_reader_threads =
      process_reader_.Threads();
  for (const ProcessReader::Thread& process_reader_thread :
       process_reader_threads) {
    auto thread = make_scoped_ptr(new internal::ThreadSnapshotMac());
    if (thread->Initialize(&process_reader_, process_reader_thread)) {
      threads_.push_back(thread.release());
    }
  }
}

void ProcessSnapshotMac::InitializeModules() {
  const std::vector<ProcessReader::Module>& process_reader_modules =
      process_reader_.Modules();
  for (const ProcessReader::Module& process_reader_module :
       process_reader_modules) {
    auto module = make_scoped_ptr(new internal::ModuleSnapshotMac());
    if (module->Initialize(&process_reader_, process_reader_module)) {
      modules_.push_back(module.release());
    }
  }
}

}  // namespace crashpad
