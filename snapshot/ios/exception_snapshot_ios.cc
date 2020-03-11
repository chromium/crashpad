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

#include "snapshot/ios/exception_snapshot_ios.h"

#include "base/logging.h"
#include "base/mac/mach_logging.h"
#include "base/strings/stringprintf.h"
#include "util/misc/from_pointer_cast.h"

#if defined(ARCH_CPU_ARM64)
using ThreadStateType = arm_thread_state64_t;
using FloatStateType = arm_neon_state64_t;
using DebugStateType = arm_debug_state64_t;
#else
using ThreadStateType = x86_thread_state64_t;
using FloatStateType = x86_float_state64_t;
using DebugStateType = x86_debug_state64_t;
#endif

namespace {

// TODO(justincohen): Share, duh.
void InitializeCPUContext(CPUContextType* type_context,
                          ThreadStateType* thread_state,
                          FloatStateType* float_state) {
#if defined(ARCH_CPU_ARM64)
  memcpy(type_context->regs, thread_state->__x, sizeof(type_context->regs));
  type_context->sp = thread_state->__sp;
  type_context->pc = thread_state->__pc;
  type_context->spsr =
      static_cast<decltype(type_context->spsr)>(thread_state->__cpsr);
  memcpy(type_context->fpsimd, float_state->__v, sizeof(float_state->__v));
  type_context->fpsr = float_state->__fpsr;
  type_context->fpcr = float_state->__fpcr;
#else
  type_context->rax = thread_state->__rax;
  type_context->rbx = thread_state->__rbx;
  type_context->rcx = thread_state->__rcx;
  type_context->rdx = thread_state->__rdx;
  type_context->rdi = thread_state->__rdi;
  type_context->rsi = thread_state->__rsi;
  type_context->rbp = thread_state->__rbp;
  type_context->rsp = thread_state->__rsp;
  type_context->r8 = thread_state->__r8;
  type_context->r9 = thread_state->__r9;
  type_context->r10 = thread_state->__r10;
  type_context->r11 = thread_state->__r11;
  type_context->r12 = thread_state->__r12;
  type_context->r13 = thread_state->__r13;
  type_context->r14 = thread_state->__r14;
  type_context->r15 = thread_state->__r15;
  type_context->rip = thread_state->__rip;
  type_context->rflags = thread_state->__rflags;
  type_context->cs = thread_state->__cs;
  type_context->fs = thread_state->__fs;
  type_context->gs = thread_state->__gs;
  memcpy(&type_context->fxsave,
         &float_state->__fpu_fcw,
         sizeof(type_context->fxsave));
#endif
}

}  // namespace

namespace crashpad {
namespace internal {

ExceptionSnapshotIOS::ExceptionSnapshotIOS()
    : ExceptionSnapshot(),
      context_(),
      codes_(),
      thread_id_(0),
      exception_address_(0),
      signal_number_(0),
      signal_code_(0),
      initialized_() {}

ExceptionSnapshotIOS::~ExceptionSnapshotIOS() {}

bool ExceptionSnapshotIOS::Initialize(const siginfo_t* siginfo,
                                      const void* context) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  //  _STRUCT_ARM_EXCEPTION_STATE64   __es;
  //  _STRUCT_ARM_THREAD_STATE64      __ss;
  //  _STRUCT_ARM_NEON_STATE64        __ns;
  mcontext_t mcontext = ((ucontext_t*)context)->uc_mcontext;
  InitializeCPUContext(&type_context_,
                       (ThreadStateType*)(&mcontext->__ss),
                       (FloatStateType*)(&mcontext->__ns));
  // What do I do with __es?
  //    __uint64_t __far;       /* Virtual Fault Address */
  //    __uint32_t __esr;       /* Exception syndrome */
  //    __uint32_t __exception; /* number of arm exception taken */

  // Thread ID.
  thread_identifier_info identifier_info;
  mach_msg_type_number_t count = THREAD_IDENTIFIER_INFO_COUNT;
  kern_return_t kr =
      thread_info(mach_thread_self(),
                  THREAD_IDENTIFIER_INFO,
                  reinterpret_cast<thread_info_t>(&identifier_info),
                  &count);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(ERROR, kr) << "thread_identifier_info";
  }
  thread_id_ = identifier_info.thread_id;

  // Shrug?
  signal_number_ = siginfo->si_signo;
  signal_code_ = siginfo->si_code;
  exception_address_ = FromPointerCast<uint64_t>(siginfo->si_addr);

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

const CPUContext* ExceptionSnapshotIOS::Context() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &context_;
}

uint64_t ExceptionSnapshotIOS::ThreadID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_id_;
}

uint32_t ExceptionSnapshotIOS::Exception() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return signal_number_;
}

uint32_t ExceptionSnapshotIOS::ExceptionInfo() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return signal_code_;
}

uint64_t ExceptionSnapshotIOS::ExceptionAddress() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return exception_address_;
}

const std::vector<uint64_t>& ExceptionSnapshotIOS::Codes() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return codes_;
}

std::vector<const MemorySnapshot*> ExceptionSnapshotIOS::ExtraMemory() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<const MemorySnapshot*>();
}

}  // namespace internal
}  // namespace crashpad
