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

#include "snapshot/ios/thread_snapshot_ios_intermediatedump.h"

#include "base/mac/mach_logging.h"
#include "snapshot/mac/cpu_context_mac.h"
#include "util/ios/ios_intermediatedump_data.h"
#include "util/ios/ios_intermediatedump_map.h"

namespace crashpad {
namespace internal {

ThreadSnapshotIOSIntermediatedump::ThreadSnapshotIOSIntermediatedump()
    : ThreadSnapshot(),
      context_(),
      stack_(),
      thread_id_(0),
      thread_specific_data_address_(0),
      suspend_count_(0),
      priority_(0),
      initialized_() {}

ThreadSnapshotIOSIntermediatedump::~ThreadSnapshotIOSIntermediatedump() {}

bool ThreadSnapshotIOSIntermediatedump::Initialize(
    const IOSIntermediatedumpMap& thread_data) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  thread_data[IntermediateDumpKey::kSuspendCount].AsData().GetData<int>(
      &suspend_count_);
  thread_data[IntermediateDumpKey::kPriority].AsData().GetData<int>(&priority_);
  thread_data[IntermediateDumpKey::kThreadID].AsData().GetData<uint64_t>(
      &thread_id_);
  thread_data[IntermediateDumpKey::kThreadDataAddress]
      .AsData()
      .GetData<uint64_t>(&thread_specific_data_address_);

  vm_address_t stack_region_address;
  thread_data[IntermediateDumpKey::kStackRegionAddress]
      .AsData()
      .GetData<vm_address_t>(&stack_region_address);
  vm_address_t stack_region_data =
      (vm_address_t)thread_data[IntermediateDumpKey::kStackRegionData]
          .AsData()
          .data();
  vm_size_t stack_region_size =
      thread_data[IntermediateDumpKey::kStackRegionData].AsData().length();
  stack_.Initialize(stack_region_address, stack_region_data, stack_region_size);

#if defined(ARCH_CPU_X86_64)
  typedef x86_thread_state64_t thread_state_type;
  typedef x86_float_state64_t float_state_type;
  typedef x86_debug_state64_t debug_state_type;
#elif defined(ARCH_CPU_ARM64)
  typedef arm_thread_state64_t thread_state_type;
  typedef arm_neon_state64_t float_state_type;
  typedef arm_debug_state64_t debug_state_type;
#endif

  thread_state_type thread_state;
  thread_data[IntermediateDumpKey::kThreadState]
      .AsData()
      .GetData<thread_state_type>(&thread_state);
  float_state_type float_state;
  thread_data[IntermediateDumpKey::kFloatState]
      .AsData()
      .GetData<float_state_type>(&float_state);
  debug_state_type debug_state;
  thread_data[IntermediateDumpKey::kDebugState]
      .AsData()
      .GetData<debug_state_type>(&debug_state);

#if defined(ARCH_CPU_X86_64)
  context_.architecture = kCPUArchitectureX86_64;
  context_.x86_64 = &context_x86_64_;
  InitializeCPUContextX86_64(&context_x86_64_,
                             THREAD_STATE_NONE,
                             nullptr,
                             0,
                             &thread_state,
                             &float_state,
                             &debug_state);
#elif defined(ARCH_CPU_ARM64)
  context_.architecture = kCPUArchitectureARM64;
  context_.arm64 = &context_arm64_;
  InitializeCPUContextARM64(&context_arm64_,
                            THREAD_STATE_NONE,
                            nullptr,
                            0,
                            &thread_state,
                            &float_state,
                            &debug_state);
#else
#error Port to your CPU architecture
#endif
  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}
const CPUContext* ThreadSnapshotIOSIntermediatedump::Context() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &context_;
}

const MemorySnapshot* ThreadSnapshotIOSIntermediatedump::Stack() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &stack_;
}

uint64_t ThreadSnapshotIOSIntermediatedump::ThreadID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_id_;
}

int ThreadSnapshotIOSIntermediatedump::SuspendCount() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return suspend_count_;
}

int ThreadSnapshotIOSIntermediatedump::Priority() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return priority_;
}

uint64_t ThreadSnapshotIOSIntermediatedump::ThreadSpecificDataAddress() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_specific_data_address_;
}

std::vector<const MemorySnapshot*>
ThreadSnapshotIOSIntermediatedump::ExtraMemory() const {
  return std::vector<const MemorySnapshot*>();
}

}  // namespace internal
}  // namespace crashpad
