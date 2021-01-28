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

#include "snapshot/ios/exception_snapshot_ios_intermediatedump.h"

#include "base/logging.h"
#include "base/mac/mach_logging.h"
#include "base/strings/stringprintf.h"
#include "snapshot/cpu_context.h"
#include "snapshot/mac/cpu_context_mac.h"
#include "util/ios/ios_intermediatedump_data.h"
#include "util/ios/ios_intermediatedump_list.h"
#include "util/ios/ios_intermediatedump_writer.h"
#include "util/misc/from_pointer_cast.h"

namespace crashpad {

namespace internal {

ExceptionSnapshotIOSIntermediatedump::ExceptionSnapshotIOSIntermediatedump()
    : ExceptionSnapshot(),
      context_(),
      codes_(),
      thread_id_(0),
      exception_address_(0),
      exception_(0),
      exception_info_(0),
      initialized_() {}

ExceptionSnapshotIOSIntermediatedump::~ExceptionSnapshotIOSIntermediatedump() {}

bool ExceptionSnapshotIOSIntermediatedump::InitializeFromSignal(
    const IOSIntermediatedumpMap& exception_data) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

#if defined(ARCH_CPU_X86_64)
  typedef x86_thread_state64_t thread_state_type;
  typedef x86_float_state64_t float_state_type;
#elif defined(ARCH_CPU_ARM64)
  typedef arm_thread_state64_t thread_state_type;
  typedef arm_neon_state64_t float_state_type;
#endif

  thread_state_type thread_state;
  exception_data[IntermediateDumpKey::kThreadState]
      .AsData()
      .GetData<thread_state_type>(&thread_state);
  float_state_type float_state;
  exception_data[IntermediateDumpKey::kFloatState]
      .AsData()
      .GetData<float_state_type>(&float_state);

#if defined(ARCH_CPU_X86_64)
  context_.architecture = kCPUArchitectureX86_64;
  context_.x86_64 = &context_x86_64_;
  x86_debug_state64_t empty_debug_state = {};
  InitializeCPUContextX86_64(&context_x86_64_,
                             THREAD_STATE_NONE,
                             nullptr,
                             0,
                             &thread_state,
                             &float_state,
                             &empty_debug_state);
#elif defined(ARCH_CPU_ARM64)
  context_.architecture = kCPUArchitectureARM64;
  context_.arm64 = &context_arm64_;
  arm_debug_state64_t empty_debug_state = {};
  InitializeCPUContextARM64(&context_arm64_,
                            THREAD_STATE_NONE,
                            nullptr,
                            0,
                            &thread_state,
                            &float_state,
                            &empty_debug_state);
#else
#error Port to your CPU architecture
#endif

  exception_data[IntermediateDumpKey::kThreadID].AsData().GetData<uint64_t>(
      &thread_id_);
  exception_data[IntermediateDumpKey::kSignalNumber].AsData().GetData<uint32_t>(
      &exception_);
  exception_data[IntermediateDumpKey::kSignalCode].AsData().GetData<uint32_t>(
      &exception_info_);
  exception_data[IntermediateDumpKey::kSignalAddress]
      .AsData()
      .GetData<uintptr_t>(&exception_address_);

  // TODO(justincohen): Record the source of the exception (signal, mach, etc).

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

bool ExceptionSnapshotIOSIntermediatedump::InitializeFromMachException(
    const IOSIntermediatedumpMap& exception_data,
    const IOSIntermediatedumpList& thread_list) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  exception_type_t exception;
  exception_data[IntermediateDumpKey::kException]
      .AsData()
      .GetData<exception_type_t>(&exception);
  codes_.push_back(exception);
  exception_ = exception;

  mach_msg_type_number_t code_count;
  exception_data[IntermediateDumpKey::kCodeCount]
      .AsData()
      .GetData<mach_msg_type_number_t>(&code_count);
  mach_exception_data_type_t* code =
      (mach_exception_data_type_t*)exception_data[IntermediateDumpKey::kCode]
          .AsData()
          .data();
  if (!code || exception_data[IntermediateDumpKey::kCode].AsData().length() !=
                   sizeof(mach_exception_data_type_t) * code_count) {
    LOG(ERROR) << "Invalid mach exception code.";
  } else {
    // TODO: rationalize with the macOS implementation.
    for (mach_msg_type_number_t code_index = 0; code_index < code_count;
         ++code_index) {
      codes_.push_back(code[code_index]);
    }
    exception_info_ = code[0];
    exception_address_ = code[1];
  }
  // For serialization, float_state and, on x86, debug_state, will be identical
  // between here and the thread_snapshot version for thread_id.  That means
  // this block getting float_state and debug_state can be skipped when doing
  // proper serialization.
#if defined(ARCH_CPU_X86_64)
  typedef x86_thread_state64_t thread_state_type;
  typedef x86_float_state64_t float_state_type;
  typedef x86_debug_state64_t debug_state_type;
#elif defined(ARCH_CPU_ARM64)
  typedef arm_thread_state64_t thread_state_type;
  typedef arm_neon_state64_t float_state_type;
  typedef arm_debug_state64_t debug_state_type;
#endif

  exception_data[IntermediateDumpKey::kThreadID].AsData().GetData<uint64_t>(
      &thread_id_);

  thread_state_type thread_state;
  float_state_type float_state;
  debug_state_type debug_state;

  for (auto& value : thread_list) {
    uint64_t other_thread_id;
    exception_data[IntermediateDumpKey::kThreadID].AsData().GetData<uint64_t>(
        &other_thread_id);
    if (thread_id_ == other_thread_id) {
      (*value)[IntermediateDumpKey::kThreadState]
          .AsData()
          .GetData<thread_state_type>(&thread_state);
      (*value)[IntermediateDumpKey::kFloatState]
          .AsData()
          .GetData<float_state_type>(&float_state);
      (*value)[IntermediateDumpKey::kDebugState]
          .AsData()
          .GetData<debug_state_type>(&debug_state);
      break;
    }
  }

  thread_state_flavor_t flavor;
  exception_data[IntermediateDumpKey::kFlavor]
      .AsData()
      .GetData<thread_state_flavor_t>(&flavor);
  size_t expected_length =
      IOSIntermediatedumpWriter::ThreadStateLengthForFlavor(flavor);
  size_t actual_length =
      exception_data[IntermediateDumpKey::kState].AsData().length();
  if (expected_length == actual_length) {
    ConstThreadState state = reinterpret_cast<ConstThreadState>(
        exception_data[IntermediateDumpKey::kState].AsData().data());
    mach_msg_type_number_t state_count;
    exception_data[IntermediateDumpKey::kStateCount]
        .AsData()
        .GetData<mach_msg_type_number_t>(&state_count);

#if defined(ARCH_CPU_X86_64)
    context_.architecture = kCPUArchitectureX86_64;
    context_.x86_64 = &context_x86_64_;
    InitializeCPUContextX86_64(&context_x86_64_,
                               flavor,
                               state,
                               state_count,
                               &thread_state,
                               &float_state,
                               &debug_state);
#elif defined(ARCH_CPU_ARM64)
    context_.architecture = kCPUArchitectureARM64;
    context_.arm64 = &context_arm64_;
    InitializeCPUContextARM64(&context_arm64_,
                              flavor,
                              state,
                              state_count,
                              &thread_state,
                              &float_state,
                              &debug_state);
#else
#error Port to your CPU architecture
#endif
  }

  // Normally, for EXC_BAD_ACCESS exceptions, the exception address is present
  // in code[1]. It may or may not be the instruction pointer address (usually
  // it’s not). code[1] may carry the exception address for other exception
  // types too, but it’s not guaranteed. But for all other exception types, the
  // instruction pointer will be the exception address, and in fact will be
  // equal to codes[1] when it’s carrying the exception address. In those cases,
  // just use the instruction pointer directly.
  bool code_1_is_exception_address = exception_ == EXC_BAD_ACCESS;

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
  if (exception_ == EXC_BAD_ACCESS &&
      (exception_info_ == EXC_I386_GPFLT ||
       exception_info_ == (VM_PROT_READ | VM_PROT_EXECUTE))) {
    code_1_is_exception_address = false;
  }
#endif

  if (!code_1_is_exception_address) {
    exception_address_ = context_.InstructionPointer();
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

bool ExceptionSnapshotIOSIntermediatedump::InitializeFromNSException(
    const IOSIntermediatedumpMap& exception_data,
    const IOSIntermediatedumpList& thread_list) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  exception_ = EXC_SOFTWARE;
  exception_info_ = 0xDEADC0DE; /* uncaught NSException */
  ;

  exception_data[IntermediateDumpKey::kThreadID].AsData().GetData<uint64_t>(
      &thread_id_);

#if defined(ARCH_CPU_X86_64)
  context_.architecture = kCPUArchitectureX86_64;
  context_.x86_64 = &context_x86_64_;
#elif defined(ARCH_CPU_ARM64)
  context_.architecture = kCPUArchitectureARM64;
  context_.arm64 = &context_arm64_;
#else
#error Port to your CPU architecture
#endif

  for (auto& value : thread_list) {
    uint64_t other_thread_id;
    (*value)[IntermediateDumpKey::kThreadID].AsData().GetData<uint64_t>(
        &other_thread_id);
    if (thread_id_ == other_thread_id) {
      uint64_t* frames =
          (uint64_t*)(*value)
              [IntermediateDumpKey::kThreadUncaughtNSExceptionFrames]
                  .AsData()
                  .data();
      size_t num_frames =
          (*value)[IntermediateDumpKey::kThreadUncaughtNSExceptionFrames]
              .AsData()
              .length() /
          sizeof(uint64_t);
      if (num_frames <= 2) {
        break;
      }

#if defined(ARCH_CPU_X86_64)
      context_x86_64_.rip = frames[0];
      context_x86_64_.rsp = frames[1];
#elif defined(ARCH_CPU_ARM64)
      context_arm64_.sp = 0;
      context_arm64_.pc = frames[0];
      context_arm64_.regs[30] = frames[1];
      context_arm64_.regs[29] = sizeof(uintptr_t);
#else
#error Port to your CPU architecture
#endif

      exception_address_ = frames[0];
      break;
    }
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

const CPUContext* ExceptionSnapshotIOSIntermediatedump::Context() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &context_;
}

uint64_t ExceptionSnapshotIOSIntermediatedump::ThreadID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_id_;
}

uint32_t ExceptionSnapshotIOSIntermediatedump::Exception() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return exception_;
}

uint32_t ExceptionSnapshotIOSIntermediatedump::ExceptionInfo() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return exception_info_;
}

uint64_t ExceptionSnapshotIOSIntermediatedump::ExceptionAddress() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return exception_address_;
}

const std::vector<uint64_t>& ExceptionSnapshotIOSIntermediatedump::Codes()
    const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return codes_;
}

std::vector<const MemorySnapshot*>
ExceptionSnapshotIOSIntermediatedump::ExtraMemory() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<const MemorySnapshot*>();
}

}  // namespace internal
}  // namespace crashpad
