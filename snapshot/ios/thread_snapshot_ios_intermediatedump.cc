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
#include "util/ios/ios_intermediatedump_list.h"
#include "util/ios/ios_intermediatedump_map.h"

#include <vector>

namespace {

std::vector<uint8_t> GenerateStackMemoryFromFrames(const uint64_t* frames,
                                                   const size_t frame_count) {
  std::vector<uint8_t> stack_memory;
  if (frame_count == 0) {
    return stack_memory;
  }
  size_t pointer_size = sizeof(uintptr_t);
  size_t frame_record_size = 2 * pointer_size;
  size_t stack_size = frame_record_size * (frame_count - 1) + pointer_size;
  stack_memory.resize(stack_size);
  uintptr_t sp = stack_size - pointer_size;
  uintptr_t fp = 0;
  uintptr_t lr = 0;
  for (size_t current_frame = frame_count - 1; current_frame > 0;
       --current_frame) {
    memcpy(&stack_memory[0] + sp, &lr, sizeof(lr));
    sp -= pointer_size;
    memcpy(&stack_memory[0] + sp, &fp, sizeof(fp));
    fp = sp;
    sp -= pointer_size;
    lr = frames[current_frame];
  }

  assert(sp == 0);  // kExpectedFinalSp
  assert(fp == sizeof(uintptr_t));  // kExpectedFinalFp
  assert(lr == frames[1]);
  return stack_memory;
}

}  // namespace
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
  float_state_type float_state;
  debug_state_type debug_state;

  if (thread_data.HasKey(
          IntermediateDumpKey::kThreadUncaughtNSExceptionFrames)) {
    auto& frame_data =
        thread_data[IntermediateDumpKey::kThreadUncaughtNSExceptionFrames]
            .AsData();
    uint64_t* frames = (uint64_t*)frame_data.data();
    size_t frame_count = frame_data.length() / sizeof(uint64_t);
    exception_stack_memory_ =
        GenerateStackMemoryFromFrames(frames, frame_count);
    stack_.Initialize(0,
                      (vm_address_t)&exception_stack_memory_[0],
                      exception_stack_memory_.size());
  } else {
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
    stack_.Initialize(
        stack_region_address, stack_region_data, stack_region_size);

    thread_data[IntermediateDumpKey::kThreadState]
        .AsData()
        .GetData<thread_state_type>(&thread_state);
    thread_data[IntermediateDumpKey::kFloatState]
        .AsData()
        .GetData<float_state_type>(&float_state);
    thread_data[IntermediateDumpKey::kDebugState]
        .AsData()
        .GetData<debug_state_type>(&debug_state);
  }

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

  const auto& thread_context_memory_regions =
      thread_data[IntermediateDumpKey::kThreadContextMemoryRegions].AsList();
  for (auto& region : thread_context_memory_regions) {
    const IOSIntermediatedumpMap& region_map = (*region).AsMap();

    vm_address_t address;
    region_map[IntermediateDumpKey::kThreadContextMemoryRegionAddress]
        .AsData()
        .GetData<vm_address_t>(&address);
    vm_size_t data_size =
        region_map[IntermediateDumpKey::kThreadContextMemoryRegionData]
            .AsData()
            .length();
    if (data_size == 0)
      continue;

    vm_address_t data =
        (vm_address_t)
            region_map[IntermediateDumpKey::kThreadContextMemoryRegionData]
                .AsData()
                .data();
    auto memory =
        std::make_unique<internal::MemorySnapshotIOSIntermediatedump>();
    memory->Initialize(address, data, data_size);
    extra_memory_.push_back(std::move(memory));
  }

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
  std::vector<const MemorySnapshot*> extra_memory;
  for (const auto& memory : extra_memory_) {
    extra_memory.push_back(memory.get());
  }
  return extra_memory;
}

}  // namespace internal
}  // namespace crashpad
