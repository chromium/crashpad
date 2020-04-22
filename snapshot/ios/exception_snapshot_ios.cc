// Copyright 2020 The Crashpad Authors. All rights reserved.
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
#include "snapshot/cpu_context.h"
#include "snapshot/ios/thread_snapshot_ios.h"
#include "snapshot/mac/cpu_context_mac.h"
#include "util/misc/from_pointer_cast.h"

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

bool ExceptionSnapshotIOS::InitializeFromSignal(const siginfo_t* siginfo,
                                                const ucontext_t* context) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  if (!context)
    return false;

  mcontext_t mcontext = context->uc_mcontext;
#if defined(ARCH_CPU_X86_64)
  context_.architecture = kCPUArchitectureX86_64;
  context_.x86_64 = &context_x86_64_;
  x86_debug_state64_t empty_debug_state;
  InitializeCPUContextX86_64(&context_x86_64_,
                             THREAD_STATE_NONE,
                             nullptr,
                             0,
                             &mcontext->__ss,
                             &mcontext->__fs,
                             &empty_debug_state);
#elif defined(ARCH_CPU_ARM64)
  context_.architecture = kCPUArchitectureARM64;
  context_.arm64 = &context_arm64_;
  InitializeCPUContextARM64(&context_arm64_, &mcontext->__ss, &mcontext->__ns);
#endif

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
  } else {
    thread_id_ = identifier_info.thread_id;
  }

  signal_number_ = siginfo->si_signo;
  signal_code_ = siginfo->si_code;
  exception_address_ = FromPointerCast<uintptr_t>(siginfo->si_addr);

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

bool ExceptionSnapshotIOS::InitializeFromMachException(
    exception_behavior_t behavior,
    thread_t exception_thread,
    exception_type_t exception,
    const mach_exception_data_type_t* code,
    mach_msg_type_number_t code_count,
    thread_state_flavor_t flavor,
    ConstThreadState state,
    mach_msg_type_number_t state_count) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  codes_.push_back(exception);
  for (mach_msg_type_number_t code_index = 0; code_index < code_count;
       ++code_index) {
    codes_.push_back(code[code_index]);
  }
  signal_number_ = exception;
  signal_code_ = code[0];

  // TODO(justincohen): When doing serialization this would simply record the
  // thread_id_ the same way ThreadSnapshotIOS captures it.  The context can be
  // recovered from the same list of thread.
  auto thread_snapshot = std::make_unique<internal::ThreadSnapshotIOS>();
  if (thread_snapshot->Initialize(exception_thread)) {
    thread_id_ = thread_snapshot->ThreadID();
    exception_thread_context_ = thread_snapshot->Context();

    // Normally, the exception address is present in code[1] for EXC_BAD_ACCESS
    // exceptions, but not for other types of exceptions.
    bool code_1_is_exception_address = signal_number_ == EXC_BAD_ACCESS;

#if defined(ARCH_CPU_X86_64)
    // For x86 and x86_64 EXC_BAD_ACCESS exceptions, some code[0] values
    // indicate that code[1] does not (or may not) carry the exception address:
    // EXC_I386_GPFLT (10.9.5 xnu-2422.115.4/osfmk/i386/trap.c user_trap() for
    // T_GENERAL_PROTECTION) and the oddball (VM_PROT_READ | VM_PROT_EXECUTE)
    // which collides with EXC_I386_BOUNDFLT (10.9.5
    // xnu-2422.115.4/osfmk/i386/fpu.c fpextovrflt()). Other EXC_BAD_ACCESS
    // exceptions come through 10.9.5 xnu-2422.115.4/osfmk/i386/trap.c
    // user_page_fault_continue() and do contain the exception address in
    // code[1].
    if (signal_number_ == EXC_BAD_ACCESS &&
        (signal_code_ == EXC_I386_GPFLT ||
         signal_code_ == (VM_PROT_READ | VM_PROT_EXECUTE))) {
      code_1_is_exception_address = false;
    }
#endif

    if (code_1_is_exception_address) {
      exception_address_ = code[1];
    } else {
      exception_address_ = thread_snapshot->Context()->InstructionPointer();
    }
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

const CPUContext* ExceptionSnapshotIOS::Context() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  if (exception_thread_context_) {
    return exception_thread_context_;
  }
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
