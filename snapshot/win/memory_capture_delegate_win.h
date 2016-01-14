 // Copyright 2016 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_SNAPSHOT_WIN_MEMORY_CAPTURE_DELEGATE_WIN_H_
#define CRASHPAD_SNAPSHOT_WIN_MEMORY_CAPTURE_DELEGATE_WIN_H_

#include "snapshot/capture_referenced_memory.h"

#include "snapshot/win/process_reader_win.h"
#include "util/stdlib/pointer_container.h"

namespace crashpad {
namespace internal {

class MemorySnapshotWin;

class MemoryCaptureDelegateWin : public MemoryCaptureDelegate {
 public:
  //! \brief A MemoryCaptureDelegate for Windows.
  //!
  //! \param[in] process_reader A ProcessReaderWin for the target process.
  //! \param[in] thread The thread being inspected. Memory ranges overlapping
  //!     this thread's stack will be ignored on the assumption that they're
  //!     already otherwise captured.
  //! \param[in] into A vector of MemorySnapshotWin to which the captured memory
  //!     will be added.
  MemoryCaptureDelegateWin(ProcessReaderWin* process_reader,
                           const ProcessReaderWin::Thread& thread,
                           PointerVector<MemorySnapshotWin>* into);

  // MemoryCaptureDelegate:
  bool Is64Bit() const override;
  bool ReadMemory(uint64_t at, uint64_t num_bytes, void* into) const override;
  std::vector<CheckedRange<uint64_t>> GetReadableRanges(
      const CheckedRange<uint64_t, uint64_t>& range) const override;
  void AddNewMemorySnapshot(const CheckedRange<uint64_t, uint64_t>& range);

 private:
  CheckedRange<uint64_t, uint64_t> stack_;
  ProcessReaderWin* process_reader_;
  PointerVector<MemorySnapshotWin>* into_;
};

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_WIN_MEMORY_CAPTURE_DELEGATE_WIN_H_
