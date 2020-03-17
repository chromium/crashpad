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

  //! \brief Returns an array of thread_t threads.
  //!
  //! \param[out] count The number of threads returned.
  //!
  //! \return An array of of size \a count threads.
  static thread_act_array_t GetThreads(mach_msg_type_number_t* count);

  // ThreadSnapshot:
  const CPUContext* Context() const override;
  const MemorySnapshot* Stack() const override;
  uint64_t ThreadID() const override;
  int SuspendCount() const override;
  int Priority() const override;
  uint64_t ThreadSpecificDataAddress() const override;
  std::vector<const MemorySnapshot*> ExtraMemory() const override;

 private:
  //! \brief Calculates the base address and size of the region used as a
  //!     thread’s stack.
  //!
  //! The region returned by this method may be formed by merging multiple
  //! adjacent regions in a process’ memory map if appropriate. The base address
  //! of the returned region may be lower than the \a stack_pointer passed in
  //! when the ABI mandates a red zone below the stack pointer.
  //!
  //! \param[in] stack_pointer The stack pointer, referring to the top (lowest
  //!     address) of a thread’s stack.
  //! \param[out] stack_region_size The size of the memory region used as the
  //!     thread’s stack.
  //!
  //! \return The base address (lowest address) of the memory region used as the
  //!     thread’s stack.
  vm_address_t CalculateStackRegion(vm_address_t stack_pointer,
                                    vm_size_t* stack_region_size);

  //! \brief Adjusts the region for the red zone, if the ABI requires one.
  //!
  //! This method performs red zone calculation for CalculateStackRegion(). Its
  //! parameters are local variables used within that method, and may be
  //! modified as needed.
  //!
  //! Where a red zone is required, the region of memory captured for a thread’s
  //! stack will be extended to include the red zone below the stack pointer,
  //! provided that such memory is mapped, readable, and has the correct user
  //! tag value. If these conditions cannot be met fully, as much of the red
  //! zone will be captured as is possible while meeting these conditions.
  //!
  //! \param[in,out] start_address The base address of the region to begin
  //!     capturing stack memory from. On entry, \a start_address is the stack
  //!     pointer. On return, \a start_address may be decreased to encompass a
  //!     red zone.
  //! \param[in,out] region_base The base address of the region that contains
  //!     stack memory. This is distinct from \a start_address in that \a
  //!     region_base will be page-aligned. On entry, \a region_base is the
  //!     base address of a region that contains \a start_address. On return,
  //!     if \a start_address is decremented and is outside of the region
  //!     originally described by \a region_base, \a region_base will also be
  //!     decremented appropriately.
  //! \param[in,out] region_size The size of the region that contains stack
  //!     memory. This region begins at \a region_base. On return, if \a
  //!     region_base is decremented, \a region_size will be incremented
  //!     appropriately.
  //! \param[in] user_tag The Mach VM system’s user tag for the region described
  //!     by the initial values of \a region_base and \a region_size. The red
  //!     zone will only be allowed to extend out of the region described by
  //!     these initial values if the user tag is appropriate for stack memory
  //!     and the expanded region has the same user tag value.
  void LocateRedZone(vm_address_t* const start_address,
                     vm_address_t* const region_base,
                     vm_address_t* const region_size,
                     const unsigned int user_tag);

#if defined(ARCH_CPU_X86_64)
  CPUContextX86_64 context_x86_64_;
#elif defined(ARCH_CPU_ARM64)
  CPUContextARM64 context_arm64_;
#else
#error Port.
#endif  // ARCH_CPU_X86_64
  CPUContext context_;
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
