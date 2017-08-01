// Copyright 2017 The Crashpad Authors. All rights reserved.
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

#include "snapshot/linux/exception_snapshot_linux.h"

#include <signal.h>

#include "base/logging.h"
#include "snapshot/linux/cpu_context_linux.h"
#include "snapshot/linux/process_reader.h"
#include "snapshot/linux/signal_context.h"
#include "util/linux/traits.h"
#include "util/misc/reinterpret_bytes.h"
#include "util/numeric/safe_assignment.h"

namespace crashpad {
namespace internal {

ExceptionSnapshotLinux::ExceptionSnapshotLinux()
    : ExceptionSnapshot(),
      context_union_(),
      context_(),
      codes_(),
      thread_id_(0),
      exception_address_(0),
      signal_number_(0),
      signal_code_(0),
      initialized_() {}

ExceptionSnapshotLinux::~ExceptionSnapshotLinux() {}

#if defined(ARCH_CPU_X86_FAMILY)
template <>
bool ExceptionSnapshotLinux::ReadContext<ContextTraits32>(
    ProcessReader* reader,
    LinuxVMAddress context_address) {
  UContext<ContextTraits32> ucontext;
  if (!reader->Memory()->Read(context_address, sizeof(ucontext), &ucontext)) {
    LOG(ERROR) << "Couldn't read ucontext";
    return false;
  }

  context_.architecture = kCPUArchitectureX86;
  context_.x86 = &context_union_.x86;

  if (ucontext.fprs.magic == X86_FXSR_MAGIC) {
    if (!reader->Memory()->Read(context_address +
                                    offsetof(UContext<ContextTraits32>, fprs) +
                                    offsetof(SignalFloatContext32, fxsave),
                                sizeof(CPUContextX86::Fxsave),
                                &context_.x86->fxsave)) {
      LOG(ERROR) << "Couldn't read fxsave";
      return false;
    }
    InitializeCPUContextX86_NoFloatingPoint(ucontext.mcontext.gprs,
                                            context_.x86);

  } else {
    DCHECK_EQ(ucontext.fprs.magic, 0xffff);
    InitializeCPUContextX86(
        ucontext.mcontext.gprs, ucontext.fprs, context_.x86);
  }
  return true;
}

template <>
bool ExceptionSnapshotLinux::ReadContext<ContextTraits64>(
    ProcessReader* reader,
    LinuxVMAddress context_address) {
  UContext<ContextTraits64> ucontext;
  if (!reader->Memory()->Read(context_address, sizeof(ucontext), &ucontext)) {
    LOG(ERROR) << "Couldn't read ucontext";
    return false;
  }

  context_.architecture = kCPUArchitectureX86_64;
  context_.x86_64 = &context_union_.x86_64;

  InitializeCPUContextX86_64(
      ucontext.mcontext.gprs, ucontext.fprs, context_.x86_64);
  return true;
}
#endif  // ARCH_CPU_X86_FAMILY

bool ExceptionSnapshotLinux::Initialize(ProcessReader* process_reader,
                                        LinuxVMAddress siginfo_address,
                                        LinuxVMAddress context_address,
                                        pid_t thread_id) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  thread_id_ = thread_id;

  if (process_reader->Is64Bit()) {
    if (!ReadContext<ContextTraits64>(process_reader, context_address) ||
        !ReadSiginfo<Traits64>(process_reader, siginfo_address)) {
      return false;
    }
  } else {
    if (!ReadContext<ContextTraits32>(process_reader, context_address) ||
        !ReadSiginfo<Traits32>(process_reader, siginfo_address)) {
      return false;
    }
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

template <typename Traits>
bool ExceptionSnapshotLinux::ReadSiginfo(ProcessReader* reader,
                                         LinuxVMAddress siginfo_address) {
  Siginfo<Traits> siginfo;
  if (!reader->Memory()->Read(siginfo_address, sizeof(siginfo), &siginfo)) {
    LOG(ERROR) << "Couldn't read siginfo";
    return false;
  }

  signal_number_ = siginfo.signo;
  signal_code_ = siginfo.code;

  uint64_t extra_code;
#define PUSH_CODE(value)                         \
  do {                                           \
    if (!ReinterpretBytes(value, &extra_code)) { \
      LOG(ERROR) << "bad code";                  \
      return false;                              \
    }                                            \
    codes_.push_back(extra_code);                \
  } while (false)

  switch (siginfo.signo) {
    case SIGILL:
    case SIGFPE:
    case SIGSEGV:
    case SIGBUS:
    case SIGTRAP:
      exception_address_ = siginfo.address;
      break;

    case SIGPOLL:  // SIGIO
      PUSH_CODE(siginfo.band);
      PUSH_CODE(siginfo.fd);
      break;

    case SIGSYS:
      exception_address_ = siginfo.call_address;
      PUSH_CODE(siginfo.syscall);
      PUSH_CODE(siginfo.arch);
      break;

    case SIGALRM:
    case SIGVTALRM:
    case SIGPROF:
      PUSH_CODE(siginfo.timerid);
      PUSH_CODE(siginfo.overrun);
      PUSH_CODE(siginfo.sigval.sigval);
      break;

    case SIGABRT:
    case SIGQUIT:
    case SIGXCPU:
    case SIGXFSZ:
    case SIGHUP:
    case SIGINT:
    case SIGPIPE:
    case SIGTERM:
    case SIGUSR1:
    case SIGUSR2:
#if defined(SIGEMT)
    case SIGEMT:
#endif  // SIGEMT
#if defined(SIGPWR)
    case SIGPWR:
#endif  // SIGPWR
#if defined(SIGSTKFLT)
    case SIGSTKFLT:
#endif  // SIGSTKFLT
      PUSH_CODE(siginfo.pid);
      PUSH_CODE(siginfo.uid);
      PUSH_CODE(siginfo.sigval.sigval);
      break;

    default:
      LOG(WARNING) << "Unhandled signal " << siginfo.signo;
  }

  return true;
}

const CPUContext* ExceptionSnapshotLinux::Context() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &context_;
}

uint64_t ExceptionSnapshotLinux::ThreadID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_id_;
}

uint32_t ExceptionSnapshotLinux::Exception() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return signal_number_;
}

uint32_t ExceptionSnapshotLinux::ExceptionInfo() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return signal_code_;
}

uint64_t ExceptionSnapshotLinux::ExceptionAddress() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return exception_address_;
}

const std::vector<uint64_t>& ExceptionSnapshotLinux::Codes() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return codes_;
}

std::vector<const MemorySnapshot*> ExceptionSnapshotLinux::ExtraMemory() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<const MemorySnapshot*>();
}

}  // namespace internal
}  // namespace crashpad
