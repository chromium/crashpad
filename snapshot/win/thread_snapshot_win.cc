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

#include "snapshot/win/thread_snapshot_win.h"

#include "base/logging.h"
#include "snapshot/win/process_reader_win.h"

namespace crashpad {
namespace internal {

ThreadSnapshotWin::ThreadSnapshotWin()
    : ThreadSnapshot(), context_(), stack_(), thread_(), initialized_() {
}

ThreadSnapshotWin::~ThreadSnapshotWin() {
}

bool ThreadSnapshotWin::Initialize(
    ProcessReaderWin* process_reader,
    const ProcessReaderWin::Thread& process_reader_thread) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  thread_ = process_reader_thread;
  stack_.Initialize(
      process_reader, thread_.stack_region_address, thread_.stack_region_size);

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

const CPUContext* ThreadSnapshotWin::Context() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  LOG(ERROR) << "TODO(scottmg): CPUContext";
  return &context_;
}

const MemorySnapshot* ThreadSnapshotWin::Stack() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &stack_;
}

uint64_t ThreadSnapshotWin::ThreadID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_.id;
}

int ThreadSnapshotWin::SuspendCount() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_.suspend_count;
}

int ThreadSnapshotWin::Priority() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_.priority;
}

uint64_t ThreadSnapshotWin::ThreadSpecificDataAddress() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_.teb;
}

}  // namespace internal
}  // namespace crashpad
