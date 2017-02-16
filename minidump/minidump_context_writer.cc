// Copyright 2014 The Crashpad Authors. All rights reserved.
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

#include "minidump/minidump_context_writer.h"

#include <windows.h>
#include <dbghelp.h>
#include <stdint.h>
#include <string.h>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "snapshot/cpu_context.h"
#include "util/file/file_writer.h"

namespace crashpad {

namespace {

// Sanity-check complex structures to ensure interoperability.
static_assert(sizeof(MinidumpContextX86) == 716, "MinidumpContextX86 size");
static_assert(sizeof(MinidumpContextAMD64) == 1232,
              "MinidumpContextAMD64 size");

// These structures can also be checked against definitions in the Windows SDK.
#if defined(OS_WIN)
#if defined(ARCH_CPU_X86_FAMILY)
static_assert(sizeof(MinidumpContextX86) == sizeof(WOW64_CONTEXT),
              "WOW64_CONTEXT size");
#if defined(ARCH_CPU_X86)
static_assert(sizeof(MinidumpContextX86) == sizeof(CONTEXT), "CONTEXT size");
#elif defined(ARCH_CPU_X86_64)
static_assert(sizeof(MinidumpContextAMD64) == sizeof(CONTEXT), "CONTEXT size");
#endif
#endif  // ARCH_CPU_X86_FAMILY
#endif  // OS_WIN

}  // namespace

MinidumpContextWriter::~MinidumpContextWriter() {
}

// static
std::unique_ptr<MinidumpContextWriter>
MinidumpContextWriter::CreateFromSnapshot(const CPUContext* context_snapshot) {
  std::unique_ptr<MinidumpContextWriter> context;

  switch (context_snapshot->architecture) {
    case kCPUArchitectureX86: {
      MinidumpContextX86Writer* context_x86 = new MinidumpContextX86Writer();
      context.reset(context_x86);
      context_x86->InitializeFromSnapshot(context_snapshot->x86);
      break;
    }

    case kCPUArchitectureX86_64: {
      MSVC_PUSH_DISABLE_WARNING(4316);  // Object on heap may not be aligned.
      MinidumpContextAMD64Writer* context_amd64 =
          new MinidumpContextAMD64Writer();
      MSVC_POP_WARNING();  // C4316
      context.reset(context_amd64);
      context_amd64->InitializeFromSnapshot(context_snapshot->x86_64);
      break;
    }

    default: {
      LOG(ERROR) << "unknown context architecture "
                 << context_snapshot->architecture;
      break;
    }
  }

  return context;
}

size_t MinidumpContextWriter::SizeOfObject() {
  DCHECK_GE(state(), kStateFrozen);

  return ContextSize();
}

MinidumpContextX86Writer::MinidumpContextX86Writer()
    : MinidumpContextWriter(), context_() {
  context_.context_flags = kMinidumpContextX86;
}

MinidumpContextX86Writer::~MinidumpContextX86Writer() {
}


void MinidumpContextX86Writer::InitializeFromSnapshot(
    const CPUContextX86* context_snapshot) {
  DCHECK_EQ(state(), kStateMutable);
  DCHECK_EQ(context_.context_flags, kMinidumpContextX86);

  context_.context_flags = kMinidumpContextX86All;

  context_.dr0 = context_snapshot->dr0;
  context_.dr1 = context_snapshot->dr1;
  context_.dr2 = context_snapshot->dr2;
  context_.dr3 = context_snapshot->dr3;
  context_.dr6 = context_snapshot->dr6;
  context_.dr7 = context_snapshot->dr7;

  // The contents of context_.fsave effectively alias everything in
  // context_.fxsave that’s related to x87 FPU state. context_.fsave doesn’t
  // carry state specific to SSE (or later), such as mxcsr and the xmm
  // registers.
  CPUContextX86::FxsaveToFsave(context_snapshot->fxsave, &context_.fsave);

  context_.gs = context_snapshot->gs;
  context_.fs = context_snapshot->fs;
  context_.es = context_snapshot->es;
  context_.ds = context_snapshot->ds;
  context_.edi = context_snapshot->edi;
  context_.esi = context_snapshot->esi;
  context_.ebx = context_snapshot->ebx;
  context_.edx = context_snapshot->edx;
  context_.ecx = context_snapshot->ecx;
  context_.eax = context_snapshot->eax;
  context_.ebp = context_snapshot->ebp;
  context_.eip = context_snapshot->eip;
  context_.cs = context_snapshot->cs;
  context_.eflags = context_snapshot->eflags;
  context_.esp = context_snapshot->esp;
  context_.ss = context_snapshot->ss;

  // This is effectively a memcpy() of a big structure.
  context_.fxsave = context_snapshot->fxsave;
}

bool MinidumpContextX86Writer::WriteObject(FileWriterInterface* file_writer) {
  DCHECK_EQ(state(), kStateWritable);

  return file_writer->Write(&context_, sizeof(context_));
}

size_t MinidumpContextX86Writer::ContextSize() const {
  DCHECK_GE(state(), kStateFrozen);

  return sizeof(context_);
}

MinidumpContextAMD64Writer::MinidumpContextAMD64Writer()
    : MinidumpContextWriter(), context_() {
  context_.context_flags = kMinidumpContextAMD64;
}

MinidumpContextAMD64Writer::~MinidumpContextAMD64Writer() {
}

void MinidumpContextAMD64Writer::InitializeFromSnapshot(
    const CPUContextX86_64* context_snapshot) {
  DCHECK_EQ(state(), kStateMutable);
  DCHECK_EQ(context_.context_flags, kMinidumpContextAMD64);

  context_.context_flags = kMinidumpContextAMD64All;

  context_.mx_csr = context_snapshot->fxsave.mxcsr;
  context_.cs = context_snapshot->cs;
  context_.fs = context_snapshot->fs;
  context_.gs = context_snapshot->gs;
  // The top 32 bits of rflags are reserved/unused.
  context_.eflags = static_cast<uint32_t>(context_snapshot->rflags);
  context_.dr0 = context_snapshot->dr0;
  context_.dr1 = context_snapshot->dr1;
  context_.dr2 = context_snapshot->dr2;
  context_.dr3 = context_snapshot->dr3;
  context_.dr6 = context_snapshot->dr6;
  context_.dr7 = context_snapshot->dr7;
  context_.rax = context_snapshot->rax;
  context_.rcx = context_snapshot->rcx;
  context_.rdx = context_snapshot->rdx;
  context_.rbx = context_snapshot->rbx;
  context_.rsp = context_snapshot->rsp;
  context_.rbp = context_snapshot->rbp;
  context_.rsi = context_snapshot->rsi;
  context_.rdi = context_snapshot->rdi;
  context_.r8 = context_snapshot->r8;
  context_.r9 = context_snapshot->r9;
  context_.r10 = context_snapshot->r10;
  context_.r11 = context_snapshot->r11;
  context_.r12 = context_snapshot->r12;
  context_.r13 = context_snapshot->r13;
  context_.r14 = context_snapshot->r14;
  context_.r15 = context_snapshot->r15;
  context_.rip = context_snapshot->rip;

  // This is effectively a memcpy() of a big structure.
  context_.fxsave = context_snapshot->fxsave;
}

size_t MinidumpContextAMD64Writer::Alignment() {
  DCHECK_GE(state(), kStateFrozen);

  // Match the alignment of MinidumpContextAMD64.
  return 16;
}

bool MinidumpContextAMD64Writer::WriteObject(FileWriterInterface* file_writer) {
  DCHECK_EQ(state(), kStateWritable);

  return file_writer->Write(&context_, sizeof(context_));
}

size_t MinidumpContextAMD64Writer::ContextSize() const {
  DCHECK_GE(state(), kStateFrozen);

  return sizeof(context_);
}

}  // namespace crashpad
