// Copyright 2018 The Crashpad Authors. All rights reserved.
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

#include "snapshot/fuchsia/exception_snapshot_fuchsia.h"

#include "snapshot/fuchsia/cpu_context_fuchsia.h"
#include "snapshot/fuchsia/process_reader_fuchsia.h"

namespace crashpad {
namespace internal {

ExceptionSnapshotFuchsia::ExceptionSnapshotFuchsia() = default;
ExceptionSnapshotFuchsia::~ExceptionSnapshotFuchsia() = default;

void ExceptionSnapshotFuchsia::Initialize(
    ProcessReaderFuchsia* process_reader,
    uint32_t exception_type,
    zx_koid_t thread_id,
    const zx_exception_report_t& exception_report) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  exception_ = exception_type;
  thread_id_ = thread_id;

  // TODO(scottmg): Not sure whether these values for exception_info_ are
  // helpful or correct. Other values in the structures are stored below into
  // Codes() in case they are useful.
#if defined(ARCH_CPU_X86_64)
  exception_info_ = exception_report.context.arch.u.x86_64.err_code;
#elif defined(ARCH_CPU_ARM64)
  exception_info_ = exception_report.context.arch.u.arm_64.esr;
#endif

  codes_.push_back(exception_);
  codes_.push_back(exception_info_);

#if defined(ARCH_CPU_X86_64)
  codes_.push_back(exception_report.context.arch.u.x86_64.vector);
  codes_.push_back(exception_report.context.arch.u.x86_64.cr2);
#elif defined(ARCH_CPU_ARM64)
  codes_.push_back(exception_report.context.arch.u.x86_64.far);
#endif

  exception_address_ = 0;
  for (const auto& t : process_reader->Threads()) {
    if (t.id == thread_id) {
#if defined(ARCH_CPU_X86_64)
      context_.architecture = kCPUArchitectureX86_64;
      context_.x86_64 = &context_arch_;
      // TODO(scottmg): Float context, once Fuchsia has a debug API to capture
      // floating point registers. ZX-1750 upstream.
      InitializeCPUContextX86_64(t.general_registers, context_.x86_64);
#elif defined(ARCH_CPU_ARM64)
      context_.architecture = kCPUArchitectureARM64;
      context_.arm64 = &context_arch_;
      // TODO(scottmg): Implement context capture for arm64.
#else
#error Port.
#endif

      // There's no fault address in the exception record, so use the PC of the
      // thread that faulted.
      exception_address_ = context_.InstructionPointer();
    }
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
}

const CPUContext* ExceptionSnapshotFuchsia::Context() const {
  return &context_;
}

uint64_t ExceptionSnapshotFuchsia::ThreadID() const {
  return thread_id_;
}

uint32_t ExceptionSnapshotFuchsia::Exception() const {
  return exception_;
}

uint32_t ExceptionSnapshotFuchsia::ExceptionInfo() const {
  return exception_info_;
}

uint64_t ExceptionSnapshotFuchsia::ExceptionAddress() const {
  return exception_address_;
}

const std::vector<uint64_t>& ExceptionSnapshotFuchsia::Codes() const {
  return codes_;
}

std::vector<const MemorySnapshot*> ExceptionSnapshotFuchsia::ExtraMemory()
    const {
  return std::vector<const MemorySnapshot*>();
}

}  // namespace internal
}  // namespace crashpad
