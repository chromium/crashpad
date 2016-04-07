// Copyright 2015 The Crashpad Authors. All rights reserved.
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

#include "snapshot/win/exception_snapshot_win.h"

#include "snapshot/capture_memory.h"
#include "snapshot/memory_snapshot.h"
#include "snapshot/win/cpu_context_win.h"
#include "snapshot/win/capture_memory_delegate_win.h"
#include "snapshot/win/memory_snapshot_win.h"
#include "util/win/nt_internals.h"

namespace crashpad {
namespace internal {

ExceptionSnapshotWin::ExceptionSnapshotWin()
    : ExceptionSnapshot(),
      context_union_(),
      context_(),
      codes_(),
      thread_id_(0),
      exception_address_(0),
      exception_flags_(0),
      exception_code_(0),
      initialized_() {
}

ExceptionSnapshotWin::~ExceptionSnapshotWin() {
}

bool ExceptionSnapshotWin::Initialize(ProcessReaderWin* process_reader,
                                      DWORD thread_id,
                                      WinVMAddress exception_pointers_address) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  const auto* thread = InitializeThreadID(process_reader, thread_id);
  if (!thread)
    return false;

#if defined(ARCH_CPU_32_BITS)
  const bool is_64_bit = false;
  using Context32 = CONTEXT;
#elif defined(ARCH_CPU_64_BITS)
  const bool is_64_bit = process_reader->Is64Bit();
  using Context32 = WOW64_CONTEXT;
  if (is_64_bit) {
    CONTEXT context_record;
    if (!InitializeFromExceptionPointers<EXCEPTION_RECORD64,
                                         process_types::EXCEPTION_POINTERS64>(
            *process_reader, exception_pointers_address, &context_record)) {
      return false;
    }
    context_.architecture = kCPUArchitectureX86_64;
    context_.x86_64 = &context_union_.x86_64;
    InitializeX64Context(context_record, context_.x86_64);
  }
#endif
  if (!is_64_bit) {
    Context32 context_record;
    if (!InitializeFromExceptionPointers<EXCEPTION_RECORD32,
                                         process_types::EXCEPTION_POINTERS32>(
            *process_reader, exception_pointers_address, &context_record)) {
      return false;
    }
    context_.architecture = kCPUArchitectureX86;
    context_.x86 = &context_union_.x86;
    InitializeX86Context(context_record, context_.x86);
  }

  CaptureReferencedMemory(process_reader, *thread);

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

bool ExceptionSnapshotWin::InitializeFabricated(
    ProcessReaderWin* process_reader,
    DWORD thread_id,
    DWORD exception_code) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  const auto* thread = InitializeThreadID(process_reader, thread_id);
  if (!thread)
    return false;

#if defined(ARCH_CPU_X86_64)
  if (process_reader->Is64Bit()) {
    context_.architecture = kCPUArchitectureX86_64;
    context_.x86_64 = &context_union_.x86_64;
    InitializeX64Context(thread->context.native, context_.x86_64);
  } else {
    context_.architecture = kCPUArchitectureX86;
    context_.x86 = &context_union_.x86;
    InitializeX86Context(thread->context.wow64, context_.x86);
  }
#else
  context_.architecture = kCPUArchitectureX86;
  context_.x86 = &context_union_.x86;
  InitializeX86Context(thread->context.native, context_.x86);
#endif  // ARCH_CPU_X86_64

  exception_address_ = context_.InstructionPointer();
  exception_code_ = exception_code;

  CaptureReferencedMemory(process_reader, *thread);

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

const CPUContext* ExceptionSnapshotWin::Context() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &context_;
}

uint64_t ExceptionSnapshotWin::ThreadID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_id_;
}

uint32_t ExceptionSnapshotWin::Exception() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return exception_code_;
}

uint32_t ExceptionSnapshotWin::ExceptionInfo() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return exception_flags_;
}

uint64_t ExceptionSnapshotWin::ExceptionAddress() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return exception_address_;
}

const std::vector<uint64_t>& ExceptionSnapshotWin::Codes() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return codes_;
}

std::vector<const MemorySnapshot*> ExceptionSnapshotWin::ExtraMemory() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  std::vector<const MemorySnapshot*> result;
  result.reserve(extra_memory_.size());
  for (const auto& em : extra_memory_)
    result.push_back(em);
  return result;
}

template <class ExceptionRecordType,
          class ExceptionPointersType,
          class ContextType>
bool ExceptionSnapshotWin::InitializeFromExceptionPointers(
    const ProcessReaderWin& process_reader,
    WinVMAddress exception_pointers_address,
    ContextType* context_record) {
  ExceptionPointersType exception_pointers;
  if (!process_reader.ReadMemory(exception_pointers_address,
                                 sizeof(exception_pointers),
                                 &exception_pointers)) {
    LOG(ERROR) << "EXCEPTION_POINTERS read failed";
    return false;
  }
  if (!exception_pointers.ExceptionRecord) {
    LOG(ERROR) << "null ExceptionRecord";
    return false;
  }

  ExceptionRecordType first_record;
  if (!process_reader.ReadMemory(
          static_cast<WinVMAddress>(exception_pointers.ExceptionRecord),
          sizeof(first_record),
          &first_record)) {
    LOG(ERROR) << "ExceptionRecord";
    return false;
  }
  exception_code_ = first_record.ExceptionCode;
  exception_flags_ = first_record.ExceptionFlags;
  exception_address_ = first_record.ExceptionAddress;
  for (DWORD i = 0; i < first_record.NumberParameters; ++i)
    codes_.push_back(first_record.ExceptionInformation[i]);
  if (first_record.ExceptionRecord) {
    // https://crashpad.chromium.org/bug/43
    LOG(WARNING) << "dropping chained ExceptionRecord";
  }

  if (!process_reader.ReadMemory(
          static_cast<WinVMAddress>(exception_pointers.ContextRecord),
          sizeof(*context_record),
          context_record)) {
    LOG(ERROR) << "ContextRecord";
    return false;
  }

  return true;
}

const ProcessReaderWin::Thread* ExceptionSnapshotWin::InitializeThreadID(
    ProcessReaderWin* process_reader,
    DWORD thread_id) {
  const ProcessReaderWin::Thread* thread = nullptr;
  for (const auto& loop_thread : process_reader->Threads()) {
    if (thread_id == loop_thread.id) {
      thread = &loop_thread;
      break;
    }
  }

  if (!thread) {
    LOG(ERROR) << "thread ID " << thread_id << " not found in process";
  } else {
    thread_id_ = thread_id;
  }

  return thread;
}

void ExceptionSnapshotWin::CaptureReferencedMemory(
    ProcessReaderWin* process_reader,
    const ProcessReaderWin::Thread& thread) {
  CaptureMemoryDelegateWin capture_memory_delegate(
      process_reader, thread, &extra_memory_);
  CaptureMemory::PointedToByContext(context_, &capture_memory_delegate);
}

}  // namespace internal
}  // namespace crashpad
