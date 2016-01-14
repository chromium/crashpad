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

#include "snapshot/win/memory_capture_delegate_win.h"

#include "snapshot/win/memory_snapshot_win.h"

namespace crashpad {
namespace internal {

MemoryCaptureDelegateWin::MemoryCaptureDelegateWin(
    ProcessReaderWin* process_reader,
    const ProcessReaderWin::Thread& thread,
    PointerVector<MemorySnapshotWin>* into)
    : stack_(thread.stack_region_address, thread.stack_region_size),
      process_reader_(process_reader),
      into_(into) {}

bool MemoryCaptureDelegateWin::Is64Bit() const {
  return process_reader_->Is64Bit();
}

bool MemoryCaptureDelegateWin::ReadMemory(uint64_t at,
                                          uint64_t num_bytes,
                                          void* into) const {
  return process_reader_->ReadMemory(at, num_bytes, into);
}

std::vector<CheckedRange<uint64_t>> MemoryCaptureDelegateWin::GetReadableRanges(
    const CheckedRange<uint64_t, uint64_t>& range) const {
  return process_reader_->GetProcessInfo().GetReadableRanges(range);
}

void MemoryCaptureDelegateWin::AddNewMemorySnapshot(
    const CheckedRange<uint64_t, uint64_t>& range) {
  // Don't bother storing this memory if it points back into the stack.
  if (stack_.ContainsRange(range))
    return;
  internal::MemorySnapshotWin* snapshot = new internal::MemorySnapshotWin();
  snapshot->Initialize(process_reader_, range.base(), range.size());
  into_->push_back(snapshot);
}

}  // namespace internal
}  // namespace crashpad
