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

#include <linux/posix_types.h>
#include <signal.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "gtest/gtest.h"
#include "snapshot/cpu_architecture.h"
#include "snapshot/linux/process_reader.h"
#include "sys/syscall.h"
#include "util/linux/address_types.h"
#include "util/misc/from_pointer_cast.h"
#include "util/misc/reinterpret_bytes.h"
#include "util/posix/signals.h"

namespace crashpad {
namespace test {
namespace {

pid_t gettid() {
  return syscall(SYS_gettid);
}

#if defined(ARCH_CPU_X86)
struct FxsaveUContext {
  ucontext_t ucontext;
  CPUContextX86::Fxsave fxsave;
};
using NativeCPUContext = FxsaveUContext;

void InitializeContext(NativeCPUContext* context) {
  context->ucontext.uc_mcontext.gregs[REG_EAX] = 0xabcd1234;
  // glibc and bionic use an unsigned long for status, but the kernel treats
  // status as two uint16_t, with the upper 16 bits called "magic" which, if set
  // to X86_FXSR_MAGIC, indicate that an fxsave follows.
  reinterpret_cast<uint16_t*>(&context->ucontext.__fpregs_mem.status)[1] =
      X86_FXSR_MAGIC;
  memset(&context->fxsave, 43, sizeof(context->fxsave));
}

void ExpectContext(const CPUContext& actual, const NativeCPUContext& expected) {
  EXPECT_EQ(actual.architecture, kCPUArchitectureX86);
  EXPECT_EQ(actual.x86->eax,
            bit_cast<uint32_t>(expected.ucontext.uc_mcontext.gregs[REG_EAX]));
  for (unsigned int byte_offset = 0; byte_offset < sizeof(actual.x86->fxsave);
       ++byte_offset) {
    SCOPED_TRACE(base::StringPrintf("byte offset = %u\n", byte_offset));
    EXPECT_EQ(reinterpret_cast<const char*>(&actual.x86->fxsave)[byte_offset],
              reinterpret_cast<const char*>(&expected.fxsave)[byte_offset]);
  }
}

#elif defined(ARCH_CPU_X86_64)
using NativeCPUContext = ucontext_t;

void InitializeContext(NativeCPUContext* context) {
  context->uc_mcontext.gregs[REG_RAX] = 0xabcd1234abcd1234;
  memset(&context->__fpregs_mem, 44, sizeof(context->__fpregs_mem));
}

void ExpectContext(const CPUContext& actual, const NativeCPUContext& expected) {
  EXPECT_EQ(actual.architecture, kCPUArchitectureX86_64);
  EXPECT_EQ(actual.x86_64->rax,
            bit_cast<uint64_t>(expected.uc_mcontext.gregs[REG_RAX]));
  for (unsigned int byte_offset = 0;
       byte_offset < sizeof(actual.x86_64->fxsave);
       ++byte_offset) {
    SCOPED_TRACE(base::StringPrintf("byte offset = %u\n", byte_offset));
    ASSERT_EQ(
        reinterpret_cast<const char*>(&actual.x86_64->fxsave)[byte_offset],
        reinterpret_cast<const char*>(&expected.__fpregs_mem)[byte_offset]);
  }
}
#else
#error Port.
#endif

TEST(ExceptionSnapshotLinux, SelfBasic) {
  ProcessReader process_reader;
  ASSERT_TRUE(process_reader.Initialize(getpid()));

  siginfo_t siginfo;
  siginfo.si_signo = SIGSEGV;
  siginfo.si_errno = 42;
  siginfo.si_code = SEGV_MAPERR;
  siginfo.si_addr = reinterpret_cast<void*>(0xdeadbeef);

  NativeCPUContext context;
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

void HandleKill(int signo, siginfo_t* siginfo, void* context) {
  ProcessReader process_reader;
  ASSERT_TRUE(process_reader.Initialize(getpid()));

  internal::ExceptionSnapshotLinux exception;
  ASSERT_TRUE(exception.Initialize(&process_reader,
                                   FromPointerCast<LinuxVMAddress>(siginfo),
                                   FromPointerCast<LinuxVMAddress>(context),
                                   gettid()));
  EXPECT_EQ(exception.Exception(), static_cast<uint32_t>(SIGUSR1));
  EXPECT_EQ(exception.Codes().size(), 3u);
  EXPECT_EQ(exception.Codes()[0], static_cast<uint64_t>(getpid()));
  EXPECT_EQ(exception.Codes()[1], getuid());
  // Codes()[2] is not set by kill, but we still expect to get it because some
  // interfaces may set it and we don't necessarily know where this signal came
  // from.
}

TEST(ExceptionSnapshotLinux, SelfKill) {
  Signals::InstallHandler(SIGUSR1, HandleKill, 0, nullptr);
  EXPECT_EQ(kill(getpid(), SIGUSR1), 0);
}

namespace timertest {

static constexpr uint32_t kSigno = SIGALRM;
static __kernel_timer_t g_timer;
static sigevent g_event;

void HandleTimer(int signo, siginfo_t* siginfo, void* context) {
  ProcessReader process_reader;
  ASSERT_TRUE(process_reader.Initialize(getpid()));

  internal::ExceptionSnapshotLinux exception;
  ASSERT_TRUE(exception.Initialize(&process_reader,
                                   FromPointerCast<LinuxVMAddress>(siginfo),
                                   FromPointerCast<LinuxVMAddress>(context),
                                   gettid()));
  EXPECT_EQ(exception.Exception(), static_cast<uint32_t>(kSigno));
  EXPECT_EQ(exception.Codes().size(), 3u);
  EXPECT_EQ(exception.Codes()[0], static_cast<uint64_t>(g_timer));
  int overruns = syscall(SYS_timer_getoverrun, g_timer);
  ASSERT_GE(overruns, 0);
  EXPECT_EQ(exception.Codes()[1], static_cast<uint64_t>(overruns));
  EXPECT_EQ(exception.Codes()[2],
            static_cast<uint64_t>(g_event.sigev_value.sival_int));
}

void Run() {
  Signals::InstallHandler(kSigno, HandleTimer, 0, nullptr);

  g_event.sigev_notify = SIGEV_SIGNAL;
  g_event.sigev_signo = kSigno;
  g_event.sigev_value.sival_int = 42;
  ASSERT_EQ(syscall(SYS_timer_create, CLOCK_MONOTONIC, &g_event, &g_timer), 0);

  itimerspec spec;
  spec.it_interval.tv_sec = 0;
  spec.it_interval.tv_nsec = 0;
  spec.it_value.tv_sec = 0;
  spec.it_value.tv_nsec = 1;
  ASSERT_EQ(syscall(SYS_timer_settime, g_timer, TIMER_ABSTIME, &spec, nullptr),
            0);
}

}  // namespace timertest

TEST(ExceptionSnapshotLinux, SelfTimer) {
  timertest::Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
