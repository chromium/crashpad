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

#include "snapshot/win/capture_referenced_memory.h"

#include <stdint.h>

#include <limits>

#include "base/memory/scoped_ptr.h"
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

  const WinVMSize kRegisterByteOffset = 256;
  const WinVMAddress target = address - kRegisterByteOffset;
  const WinVMSize size = 1024;
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

template <class T>
void CaptureAtPointersInRange(uint8_t* buffer,
                              WinVMSize buffer_size,
                              ProcessReaderWin* process_reader,
                              PointerVector<MemorySnapshotWin>* into) {
  for (WinVMAddress address_offset = 0; address_offset < buffer_size;
       address_offset += sizeof(T)) {
    WinVMAddress target_address =
        *reinterpret_cast<T*>(&buffer[address_offset]);
    MaybeCaptureMemoryAround(process_reader, target_address, into);
  }
}
void CaptureMemoryPointedToByMemoryRange(
    const MemorySnapshotWin& memory,
    ProcessReaderWin* process_reader,
    PointerVector<MemorySnapshotWin>* into) {
  if (memory.Size() == 0)
    return;

  if (process_reader->Is64Bit()) {
    if (memory.Address() % sizeof(uint64_t) != 0 ||
        memory.Size() % sizeof(uint64_t) != 0) {
      LOG(ERROR) << "unaligned range";
      return;
    }
  } else {
    if (memory.Address() % sizeof(uint32_t) != 0 ||
        memory.Size() % sizeof(uint32_t) != 0) {
      LOG(ERROR) << "unaligned range";
      return;
    }
  }

  scoped_ptr<uint8_t[]> buffer(new uint8_t[memory.Size()]);
  if (!process_reader->ReadMemory(
          memory.Address(), memory.Size(), buffer.get())) {
    LOG(ERROR) << "ReadMemory";
    return;
  }

  if (process_reader->Is64Bit()) {
    CaptureAtPointersInRange<uint64_t>(
        buffer.get(), memory.Size(), process_reader, into);
  } else {
    CaptureAtPointersInRange<uint32_t>(
        buffer.get(), memory.Size(), process_reader, into);
  }
}

}  // namespace internal
}  // namespace crashpad
