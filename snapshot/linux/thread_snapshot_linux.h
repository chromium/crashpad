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

#ifndef CRASHPAD_SNAPSHOT_LINUX_THREAD_SNAPSHOT_LINUX_H_
#define CRASHPAD_SNAPSHOT_LINUX_THREAD_SNAPSHOT_LINUX_H_

#include <stdint.h>

#include "base/macros.h"
#include "build/build_config.h"
#include "snapshot/cpu_context.h"
#include "snapshot/linux/memory_snapshot_linux.h"
#include "snapshot/linux/process_reader.h"
#include "snapshot/memory_snapshot.h"
#include "snapshot/thread_snapshot.h"
#include "util/misc/initialization_state_dcheck.h"

namespace crashpad {
namespace internal {

//! \brief A ThreadSnapshot of a thread on a Linux system.
class ThreadSnapshotLinux final : public ThreadSnapshot {
 public:
  ThreadSnapshotLinux();
  ~ThreadSnapshotLinux() override;

  //! \brief Initializes the object.
  //!
  //! \param[in] process_reader A ProcessReader for the process containing the
  //!     thread.
  //! \param[in] thread The thread within the ProcessReader for
  //!     which the snapshot should be created.
  //!
  //! \return `true` if the snapshot could be created, `false` otherwise with
  //!     a message logged.
  bool Initialize(ProcessReader* process_reader,
                  const ProcessReader::Thread& thread);

  // ThreadSnapshot:

  const CPUContext* Context() const override;
  const MemorySnapshot* Stack() const override;
  uint64_t ThreadID() const override;
  int SuspendCount() const override;
  int Priority() const override;
  uint64_t ThreadSpecificDataAddress() const override;
  std::vector<const MemorySnapshot*> ExtraMemory() const override;

 private:
#if defined(ARCH_CPU_X86_FAMILY)
  union {
    CPUContextX86 x86;
    CPUContextX86_64 x86_64;
  } context_union_;
#else
#error Port.
#endif  // ARCH_CPU_X86_FAMILY
  CPUContext context_;
  MemorySnapshotLinux stack_;
  LinuxVMAddress thread_specific_data_address_;
  pid_t thread_id_;
  int priority_;
  InitializationStateDcheck initialized_;

  DISALLOW_COPY_AND_ASSIGN(ThreadSnapshotLinux);
};

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_LINUX_THREAD_SNAPSHOT_LINUX_H_
