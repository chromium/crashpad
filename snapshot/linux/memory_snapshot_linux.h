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

#ifndef CRASHPAD_SNAPSHOT_LINUX_MEMORY_SNAPSHOT_LINUX_H_
#define CRASHPAD_SNAPSHOT_LINUX_MEMORY_SNAPSHOT_LINUX_H_

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "snapshot/linux/process_reader.h"
#include "snapshot/memory_snapshot.h"
#include "util/linux/address_types.h"
#include "util/misc/initialization_state_dcheck.h"

namespace crashpad {
namespace internal {

//! \brief A MemorySnapshot of a memory region in a process on the running
//!     system, when the system runs Linux.
class MemorySnapshotLinux final : public MemorySnapshot {
 public:
  MemorySnapshotLinux();
  ~MemorySnapshotLinux() override;

  //! \brief Initializes the object.
  //!
  //! Memory is read lazily. No attempt is made to read the memory snapshot data
  //! until Read() is called, and the memory snapshot data is discared when
  //! Read() returns.
  //!
  //! \param[in] process_reader A reader for the process being snapshotted.
  //! \param[in] address The base address of the memory region to snapshot, in
  //!     the snapshot process’ address space.
  //! \param[in] size The size of the memory region to snapshot.
  void Initialize(ProcessReader* process_reader,
                  LinuxVMAddress address,
                  size_t size);

  // MemorySnapshot:

  uint64_t Address() const override;
  size_t Size() const override;
  bool Read(Delegate* delegate) const override;

 private:
  ProcessReader* process_reader_;  // weak
  uint64_t address_;
  size_t size_;
  InitializationStateDcheck initialized_;

  DISALLOW_COPY_AND_ASSIGN(MemorySnapshotLinux);
};

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_LINUX_MEMORY_SNAPSHOT_LINUX_H_
