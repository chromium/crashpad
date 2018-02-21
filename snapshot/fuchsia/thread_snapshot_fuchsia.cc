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

#include "snapshot/fuchsia/thread_snapshot_fuchsia.h"

#include "base/logging.h"

namespace crashpad {
namespace internal {

ThreadSnapshotFuchsia::ThreadSnapshotFuchsia()
    : ThreadSnapshot(),
      context_union_(),
      context_(),
      stack_(),
      thread_specific_data_address_(0),
      thread_id_(ZX_KOID_INVALID),
      initialized_() {}

ThreadSnapshotFuchsia::~ThreadSnapshotFuchsia() {}

bool ThreadSnapshotFuchsia::Initialize(
    ProcessReader* process_reader,
    const ProcessReader::Thread& thread) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

#if defined(ARCH_CPU_X86_64)
  context_.architecture = kCPUArchitectureX86_64;
  context_.x86_64 = &context_union_.x86_64;
  InitializeCPUContextX86_64(thread.thread_info.thread_context.t64,
                             thread.thread_info.float_context.f64,
                             context_.x86_64);
#elif defined(ARCH_CPU_ARM64)
  context_.architecture = kCPUArchitectureARM64;
  context_.arm64 = &context_union_.arm64;
  InitializeCPUContextARM64(thread.thread_info.thread_context.t64,
                            thread.thread_info.float_context.f64,
                            context_.arm64);
#else
#error Port.
#endif

  stack_.Initialize(process_reader,
                    thread.stack_region_address,
                    thread.stack_region_size);

  thread_specific_data_address_ =
      thread.thread_info.thread_specific_data_address;

  thread_id_ = thread.tid;

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

const CPUContext* ThreadSnapshotFuchsia::Context() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &context_;
}

const MemorySnapshot* ThreadSnapshotFuchsia::Stack() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &stack_;
}

uint64_t ThreadSnapshotFuchsia::ThreadID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_id_;
}

int ThreadSnapshotFuchsia::SuspendCount() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return 0;
}

int ThreadSnapshotFuchsia::Priority() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return priority_;
}

uint64_t ThreadSnapshotFuchsia::ThreadSpecificDataAddress() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_specific_data_address_;
}

std::vector<const MemorySnapshot*> ThreadSnapshotFuchsia::ExtraMemory() const {
  return std::vector<const MemorySnapshot*>();
}

}  // namespace internal
}  // namespace crashpad
