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

#include "snapshot/mac/exception_snapshot_mac.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "snapshot/mac/cpu_context_mac.h"
#include "snapshot/mac/process_reader.h"
#include "util/mach/exc_server_variants.h"
#include "util/numeric/safe_assignment.h"

namespace crashpad {
namespace internal {

ExceptionSnapshotMac::ExceptionSnapshotMac()
    : ExceptionSnapshot(),
      context_union_(),
      context_(),
      codes_(),
      thread_id_(0),
      exception_address_(0),
      exception_(0),
      exception_code_0_(0),
      initialized_() {
}

ExceptionSnapshotMac::~ExceptionSnapshotMac() {
}

bool ExceptionSnapshotMac::Initialize(ProcessReader* process_reader,
                                      thread_t exception_thread,
                                      exception_type_t exception,
                                      const mach_exception_data_type_t* code,
                                      mach_msg_type_number_t code_count,
                                      thread_state_flavor_t flavor,
                                      ConstThreadState state,
                                      mach_msg_type_number_t state_count) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  codes_.push_back(exception);
  for (mach_msg_type_number_t code_index = 0;
       code_index < code_count;
       ++code_index) {
    codes_.push_back(code[code_index]);
  }

  exception_ = exception;
  mach_exception_code_t exception_code_0 = code[0];

  if (exception_ == EXC_CRASH) {
    exception_ = ExcCrashRecoverOriginalException(
        exception_code_0, &exception_code_0, nullptr);
  }

  // ExceptionInfo() returns code[0] in a 32-bit field. This shouldn’t be a
  // problem because code[0]’s values never exceed 32 bits. Only code[1] is ever
  // expected to be that wide.
  if (!AssignIfInRange(&exception_code_0_, exception_code_0)) {
    LOG(WARNING)
        << base::StringPrintf("exception_code_0 0x%llx out of range",
                              exception_code_0);
    return false;
  }

  const ProcessReader::Thread* thread = nullptr;
  for (const ProcessReader::Thread& loop_thread : process_reader->Threads()) {
    if (exception_thread == loop_thread.port) {
      thread = &loop_thread;
      break;
    }
  }

  if (!thread) {
    LOG(WARNING) << "exception_thread not found in task";
    return false;
  }

  thread_id_ = thread->id;

  // Normally, the exception address is present in code[1] for EXC_BAD_ACCESS
  // exceptions, but not for other types of exceptions.
  bool code_1_is_exception_address = exception_ == EXC_BAD_ACCESS;

#if defined(ARCH_CPU_X86_FAMILY)
  if (process_reader->Is64Bit()) {
    context_.architecture = kCPUArchitectureX86_64;
    context_.x86_64 = &context_union_.x86_64;
    InitializeCPUContextX86_64(context_.x86_64,
                               flavor,
                               state,
                               state_count,
                               &thread->thread_context.t64,
                               &thread->float_context.f64,
                               &thread->debug_context.d64);
  } else {
    context_.architecture = kCPUArchitectureX86;
    context_.x86 = &context_union_.x86;
    InitializeCPUContextX86(context_.x86,
                            flavor,
                            state,
                            state_count,
                            &thread->thread_context.t32,
                            &thread->float_context.f32,
                            &thread->debug_context.d32);
  }

  // For x86 and x86_64 EXC_BAD_ACCESS exceptions, some code[0] values indicate
  // that code[1] does not (or may not) carry the exception address:
  // EXC_I386_GPFLT (10.9.5 xnu-2422.115.4/osfmk/i386/trap.c user_trap() for
  // T_GENERAL_PROTECTION) and the oddball (VM_PROT_READ | VM_PROT_EXECUTE)
  // which collides with EXC_I386_BOUNDFLT (10.9.5
  // xnu-2422.115.4/osfmk/i386/fpu.c fpextovrflt()). Other EXC_BAD_ACCESS
  // exceptions come through 10.9.5 xnu-2422.115.4/osfmk/i386/trap.c
  // user_page_fault_continue() and do contain the exception address in code[1].
  if (exception_ == EXC_BAD_ACCESS &&
      (exception_code_0_ == EXC_I386_GPFLT ||
       exception_code_0_ == (VM_PROT_READ | VM_PROT_EXECUTE))) {
    code_1_is_exception_address = false;
  }
#endif

  if (code_1_is_exception_address) {
    exception_address_ = code[1];
  } else {
    exception_address_ = context_.InstructionPointer();
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

const CPUContext* ExceptionSnapshotMac::Context() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &context_;
}

uint64_t ExceptionSnapshotMac::ThreadID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_id_;
}

uint32_t ExceptionSnapshotMac::Exception() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return exception_;
}

uint32_t ExceptionSnapshotMac::ExceptionInfo() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return exception_code_0_;
}

uint64_t ExceptionSnapshotMac::ExceptionAddress() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return exception_address_;
}

const std::vector<uint64_t>& ExceptionSnapshotMac::Codes() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return codes_;
}

}  // namespace internal
}  // namespace crashpad
