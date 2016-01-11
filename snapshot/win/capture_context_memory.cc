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

#include "snapshot/win/capture_context_memory.h"

#include <stdint.h>

#include <limits>

#include "snapshot/win/memory_snapshot_win.h"
#include "snapshot/win/process_reader_win.h"

namespace crashpad {
namespace internal {

namespace {

void MaybeCaptureMemoryAround(ProcessReaderWin* process_reader,
                              WinVMAddress address,
                              PointerVector<MemorySnapshotWin>* into) {
  const WinVMAddress non_address_offset = 0x10000;
  if (address < non_address_offset)
    return;
  if (process_reader->Is64Bit()) {
    if (address >= std::numeric_limits<uint64_t>::max() - non_address_offset)
      return;
  } else {
    if (address >= std::numeric_limits<uint32_t>::max() - non_address_offset)
      return;
  }

  const WinVMSize kRegisterByteOffset = 32;
  const WinVMAddress target = address - kRegisterByteOffset;
  const WinVMSize size = 128;
  auto ranges = process_reader->GetProcessInfo().GetReadableRanges(
      CheckedRange<WinVMAddress, WinVMSize>(target, size));
  for (const auto& range : ranges) {
    internal::MemorySnapshotWin* snapshot = new internal::MemorySnapshotWin();
    snapshot->Initialize(process_reader, range.base(), range.size());
    into->push_back(snapshot);
  }
}

}  // namespace

void CaptureMemoryPointedToByContext(const CPUContext& context,
                                     ProcessReaderWin* process_reader,
                                     const ProcessReaderWin::Thread& thread,
                                     PointerVector<MemorySnapshotWin>* into) {
#if defined(ARCH_CPU_X86_64)
  if (context.architecture == kCPUArchitectureX86_64) {
    MaybeCaptureMemoryAround(process_reader, context.x86_64->rax, into);
    MaybeCaptureMemoryAround(process_reader, context.x86_64->rbx, into);
    MaybeCaptureMemoryAround(process_reader, context.x86_64->rcx, into);
    MaybeCaptureMemoryAround(process_reader, context.x86_64->rdx, into);
    MaybeCaptureMemoryAround(process_reader, context.x86_64->rdi, into);
    MaybeCaptureMemoryAround(process_reader, context.x86_64->rsi, into);
    if (context.x86_64->rbp < thread.stack_region_address ||
        context.x86_64->rbp >=
            thread.stack_region_address + thread.stack_region_size) {
      MaybeCaptureMemoryAround(process_reader, context.x86_64->rbp, into);
    }
    MaybeCaptureMemoryAround(process_reader, context.x86_64->r8, into);
    MaybeCaptureMemoryAround(process_reader, context.x86_64->r9, into);
    MaybeCaptureMemoryAround(process_reader, context.x86_64->r10, into);
    MaybeCaptureMemoryAround(process_reader, context.x86_64->r11, into);
    MaybeCaptureMemoryAround(process_reader, context.x86_64->r12, into);
    MaybeCaptureMemoryAround(process_reader, context.x86_64->r13, into);
    MaybeCaptureMemoryAround(process_reader, context.x86_64->r14, into);
    MaybeCaptureMemoryAround(process_reader, context.x86_64->r15, into);
    MaybeCaptureMemoryAround(process_reader, context.x86_64->rip, into);
  } else {
#endif
    MaybeCaptureMemoryAround(process_reader, context.x86->eax, into);
    MaybeCaptureMemoryAround(process_reader, context.x86->ebx, into);
    MaybeCaptureMemoryAround(process_reader, context.x86->ecx, into);
    MaybeCaptureMemoryAround(process_reader, context.x86->edx, into);
    MaybeCaptureMemoryAround(process_reader, context.x86->edi, into);
    MaybeCaptureMemoryAround(process_reader, context.x86->esi, into);
    if (context.x86->ebp < thread.stack_region_address ||
        context.x86->ebp >=
            thread.stack_region_address + thread.stack_region_size) {
      MaybeCaptureMemoryAround(process_reader, context.x86->ebp, into);
    }
    MaybeCaptureMemoryAround(process_reader, context.x86->eip, into);
#if defined(ARCH_CPU_X86_64)
  }
#endif
}

}  // namespace internal
}  // namespace crashpad
