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
#include <ucontext.h>

#include "base/macros.h"
#include "gtest/gtest.h"
#include "snapshot/cpu_architecture.h"
#include "snapshot/linux/process_reader.h"
#include "sys/syscall.h"
#include "util/linux/address_types.h"
#include "util/misc/from_pointer_cast.h"

namespace crashpad {
namespace test {
namespace {

pid_t gettid() {
  return syscall(SYS_gettid);
}

void InitializeContext(ucontext_t* context) {
#if defined(ARCH_CPU_X86_64)
  context->uc_mcontext.gregs[REG_RAX] = 0xabcd1234abcd1234;
#elif defined(ARCH_CPU_X86)
  context->uc_mcontext.gregs[REG_EAX] = 0xabcd1234;
#else
#error Port.
#endif
}

void ExpectContext(const CPUContext& actual, ucontext_t expected) {
#if defined(ARCH_CPU_X86_64)
  EXPECT_EQ(actual.architecture, kCPUArchitectureX86_64);
  EXPECT_EQ(actual.x86_64->rax,
            bit_cast<uint64_t>(expected.uc_mcontext.gregs[REG_RAX]));
#elif defined(ARCH_CPU_X86)
  EXPECT_EQ(actual.architecture, kCPUArchitectureX86);
  EXPECT_EQ(actual.x86->eax,
            bit_cast<uint32_t>(expected.uc_mcontext.gregs[REG_EAX]));
#else
#error Port.
#endif
}

TEST(ExceptionSnapshotLinux, SelfBasic) {
  ProcessReader process_reader;
  ASSERT_TRUE(process_reader.Initialize(getpid()));

  siginfo_t siginfo;
  siginfo.si_signo = SIGSEGV;
  siginfo.si_errno = 42;
  siginfo.si_code = SEGV_MAPERR;
  siginfo.si_addr = reinterpret_cast<void*>(0xdeadbeef);

  ucontext_t context;
  InitializeContext(&context);

  internal::ExceptionSnapshotLinux exception;
  ASSERT_TRUE(exception.Initialize(&process_reader,
                                   FromPointerCast<LinuxVMAddress>(&siginfo),
                                   FromPointerCast<LinuxVMAddress>(&context),
                                   gettid()));
  EXPECT_EQ(exception.Exception(), static_cast<uint32_t>(siginfo.si_signo));
  EXPECT_EQ(exception.ExceptionInfo(), static_cast<uint32_t>(siginfo.si_code));
  EXPECT_EQ(exception.ExceptionAddress(),
            FromPointerCast<uint64_t>(siginfo.si_addr));
  ExpectContext(*exception.Context(), context);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
