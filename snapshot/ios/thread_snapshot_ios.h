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

#ifndef CRASHPAD_SNAPSHOT_IOS_THREAD_SNAPSHOT_IOS_H_
#define CRASHPAD_SNAPSHOT_IOS_THREAD_SNAPSHOT_IOS_H_

#include "base/macros.h"
#include "build/build_config.h"
#include "snapshot/cpu_context.h"
#include "snapshot/ios/memory_snapshot_ios.h"
#include "snapshot/thread_snapshot.h"
#include "util/misc/initialization_state_dcheck.h"

#if defined(ARCH_CPU_X86_FAMILY)
using CPUContextType = crashpad::CPUContextX86_64;
#elif defined(ARCH_CPU_ARM_FAMILY)
using CPUContextType = crashpad::CPUContextARM64;
#endif  // ARCH_CPU_X86_FAMILY

namespace crashpad {
namespace internal {

//! \brief A ThreadSnapshot of a thread on an iOS system.
class ThreadSnapshotIOS final : public ThreadSnapshot {
 public:
  ThreadSnapshotIOS();
  ~ThreadSnapshotIOS() override;

  //! \brief Initializes the object.
  //!
  //! \brief thread The mach thread used to initialize this object.
  bool Initialize(thread_t thread);

  static thread_act_array_t AllThread(mach_msg_type_number_t* count);

  vm_address_t CalculateStackRegion(vm_address_t stack_pointer,
                                    vm_size_t* stack_region_size);

  void LocateRedZone(vm_address_t* const start_address,
                     vm_address_t* const region_base,
                     vm_address_t* const region_size,
                     const unsigned int user_tag);

  // ThreadSnapshot:
  const CPUContext* Context() const override;
  const MemorySnapshot* Stack() const override;
  uint64_t ThreadID() const override;
  int SuspendCount() const override;
  int Priority() const override;
  uint64_t ThreadSpecificDataAddress() const override;
  std::vector<const MemorySnapshot*> ExtraMemory() const override;

 private:
  CPUContext context_;
  CPUContextType type_context_;
  MemorySnapshotIOS stack_;
  uint64_t thread_id_;
  uint64_t thread_specific_data_address_;
  int suspend_count_;
  int priority_;
  InitializationStateDcheck initialized_;

  DISALLOW_COPY_AND_ASSIGN(ThreadSnapshotIOS);
};

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_IOS_THREAD_SNAPSHOT_IOS_H_
