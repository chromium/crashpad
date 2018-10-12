// Copyright 2018 The Crashpad Authors. All rights reserved.
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

#include "snapshot/minidump/thread_snapshot_minidump.h"

namespace crashpad {
namespace internal {

ThreadSnapshotMinidump::ThreadSnapshotMinidump()
    : ThreadSnapshot(),
      minidump_thread_(),
      initialized_() {
}

ThreadSnapshotMinidump::~ThreadSnapshotMinidump() {
}

bool ThreadSnapshotMinidump::Initialize(FileReaderInterface* file_reader,
                                        RVA minidump_thread_rva) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);
  if (!file_reader->SeekSet(minidump_thread_rva)) {
    return false;
  }

  if (!file_reader->ReadExactly(&minidump_thread_, sizeof(minidump_thread_))) {
    return false;
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

uint64_t ThreadSnapshotMinidump::ThreadID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return minidump_thread_.ThreadId;
}

int ThreadSnapshotMinidump::SuspendCount() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return minidump_thread_.SuspendCount;
}

uint64_t ThreadSnapshotMinidump::ThreadSpecificDataAddress() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return minidump_thread_.Teb;
}

int ThreadSnapshotMinidump::Priority() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return minidump_thread_.Priority;
}

const CPUContext* ThreadSnapshotMinidump::Context() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  NOTREACHED();  // https://crashpad.chromium.org/bug/10
  return nullptr;
}

const MemorySnapshot* ThreadSnapshotMinidump::Stack() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  NOTREACHED();  // https://crashpad.chromium.org/bug/10
  return nullptr;
}

std::vector<const MemorySnapshot*> ThreadSnapshotMinidump::ExtraMemory() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  NOTREACHED();  // https://crashpad.chromium.org/bug/10
  return std::vector<const MemorySnapshot*>();
}

}  // namespace internal
}  // namespace crashpad
