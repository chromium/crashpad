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

bool ExceptionSnapshotIOS::Initialize(const siginfo_t* siginfo,
                                      const ucontext_t* context) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  if (!context)
    return false;

  mcontext_t mcontext = context->uc_mcontext;
#if defined(ARCH_CPU_X86_64)
  context_.architecture = kCPUArchitectureX86_64;
  context_.x86_64 = &context_x86_64_;
  InitializeCPUContextX86_64(&context_x86_64_,
                             THREAD_STATE_NONE,
                             nullptr,
                             0,
                             &mcontext->__ss,
                             &mcontext->__fs,
                             nullptr);
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

  // Do I just do this always:
  exception_address_ = FromPointerCast<uint64_t>(siginfo->si_addr);

  // Or only for the first five signal numbers?
  switch (signal_number_) {
    case SIGILL:
    case SIGFPE:
    case SIGSEGV:
    case SIGBUS:
    case SIGTRAP:
      //      exception_address_ = FromPointerCast<uint64_t>(siginfo->si_addr);
      break;
    case SIGSYS:
      // Doesn't seem to exist on iOS
      //      exception_address_ = siginfo.call_address;
      //      PUSH_CODE(siginfo.syscall);
      //      PUSH_CODE(siginfo.arch);
      break;

    case SIGALRM:
    case SIGVTALRM:
    case SIGPROF:
      // Doesn't seem to exist on iOS
      //      PUSH_CODE(siginfo.timerid);
      //      PUSH_CODE(siginfo.overrun);
      //      PUSH_CODE(siginfo.sigval.sigval);
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
    case SIGEMT:
      // No exception address here?
      codes_.push_back(siginfo->si_pid);
      codes_.push_back(siginfo->si_uid);
      codes_.push_back(siginfo->si_value.sival_int);
      break;

    default:
      LOG(WARNING) << "Unhandled signal " << signal_number_;
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

bool ExceptionSnapshotIOS::Initialize(NSException* exception) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  signal_number_ = EXC_SOFTWARE;
  signal_code_ = 0xDEADC0DE; /* uncaught NSException, stolen from breakpad. */

  NSArray* addresses = [exception callStackReturnAddresses];
  exception_address_ =
      reinterpret_cast<uint64_t>([addresses[0] unsignedLongLongValue]);

  // Also store these in annotations.
  std::string name = [[exception name] UTF8String];
  std::string reason = [[exception reason] UTF8String];

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

#if defined(ARCH_CPU_X86_64)
  context_.architecture = kCPUArchitectureX86_64;
  context_.x86_64 = &context_x86_64_;
  context_x86_64_.rip = exception_address_;
#elif defined(ARCH_CPU_ARM64)
  context_.architecture = kCPUArchitectureARM64;
  context_.arm64 = &context_arm64_;
  context_arm64_.pc = exception_address_;
#endif

  for (NSUInteger return_address_index = 0;
       return_address_index < addresses.count;
       return_address_index++) {
    // TBD.
  }

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
