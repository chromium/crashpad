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

#include "snapshot/win/thread_snapshot_win.h"

#include "base/logging.h"
#include "snapshot/win/process_reader_win.h"

namespace crashpad {
namespace internal {

namespace {

void InitializeX64Context(const CONTEXT& context,
                          CPUContextX86_64* out) {
  out->rax = context.Rax;
  out->rbx = context.Rbx;
  out->rcx = context.Rcx;
  out->rdx = context.Rdx;
  out->rdi = context.Rdi;
  out->rsi = context.Rsi;
  out->rbp = context.Rbp;
  out->rsp = context.Rsp;
  out->r8 = context.R8;
  out->r9 = context.R9;
  out->r10 = context.R10;
  out->r11 = context.R11;
  out->r12 = context.R12;
  out->r13 = context.R13;
  out->r14 = context.R14;
  out->r15 = context.R15;
  out->rip = context.Rip;
  out->rflags = context.EFlags;
  out->cs = context.SegCs;
  out->fs = context.SegFs;
  out->gs = context.SegGs;

  out->dr0 = context.Dr0;
  out->dr1 = context.Dr1;
  out->dr2 = context.Dr2;
  out->dr3 = context.Dr3;
  // DR4 and DR5 are obsolete synonyms for DR6 and DR7, see
  // http://en.wikipedia.org/wiki/X86_debug_register.
  out->dr4 = context.Dr6;
  out->dr5 = context.Dr7;
  out->dr6 = context.Dr6;
  out->dr7 = context.Dr7;

  static_assert(sizeof(out->fxsave) == sizeof(context.FltSave),
                "types must be equivalent");
  memcpy(&out->fxsave, &context.FltSave.ControlWord, sizeof(out->fxsave));
}

void InitializeX86Context(const CONTEXT& context,
                          CPUContextX86* out) {
  CHECK(false) << "TODO(scottmg) InitializeX86Context()";
}

}  // namespace

ThreadSnapshotWin::ThreadSnapshotWin()
    : ThreadSnapshot(), context_(), stack_(), thread_(), initialized_() {
}

ThreadSnapshotWin::~ThreadSnapshotWin() {
}

bool ThreadSnapshotWin::Initialize(
    ProcessReaderWin* process_reader,
    const ProcessReaderWin::Thread& process_reader_thread) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  thread_ = process_reader_thread;
  stack_.Initialize(
      process_reader, thread_.stack_region_address, thread_.stack_region_size);

#if defined(ARCH_CPU_X86_FAMILY)
  if (process_reader->Is64Bit()) {
    context_.architecture = kCPUArchitectureX86_64;
    context_.x86_64 = &context_union_.x86_64;
    InitializeX64Context(process_reader_thread.context, context_.x86_64);
  } else {
    context_.architecture = kCPUArchitectureX86;
    context_.x86 = &context_union_.x86;
    InitializeX86Context(process_reader_thread.context, context_.x86);
  }
#endif

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

const CPUContext* ThreadSnapshotWin::Context() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &context_;
}

const MemorySnapshot* ThreadSnapshotWin::Stack() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &stack_;
}

uint64_t ThreadSnapshotWin::ThreadID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_.id;
}

int ThreadSnapshotWin::SuspendCount() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_.suspend_count;
}

int ThreadSnapshotWin::Priority() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_.priority;
}

uint64_t ThreadSnapshotWin::ThreadSpecificDataAddress() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_.teb;
}

}  // namespace internal
}  // namespace crashpad
