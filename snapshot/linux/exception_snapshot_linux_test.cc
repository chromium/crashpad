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
#include <string.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "gtest/gtest.h"
#include "snapshot/cpu_architecture.h"
#include "snapshot/linux/process_reader.h"
#include "sys/syscall.h"
#include "test/errors.h"
#include "test/linux/fake_ptrace_connection.h"
#include "util/linux/address_types.h"
#include "util/misc/clock.h"
#include "util/misc/from_pointer_cast.h"
#include "util/posix/signals.h"
#include "util/synchronization/semaphore.h"

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
    EXPECT_EQ(
        reinterpret_cast<const char*>(&actual.x86_64->fxsave)[byte_offset],
        reinterpret_cast<const char*>(&expected.__fpregs_mem)[byte_offset]);
  }
}
#else
#error Port.
#endif

TEST(ExceptionSnapshotLinux, SelfBasic) {
  FakePtraceConnection connection;
  ASSERT_TRUE(connection.Initialize(getpid()));

  ProcessReader process_reader;
  ASSERT_TRUE(process_reader.Initialize(&connection));

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

class ScopedSigactionRestore {
 public:
  ScopedSigactionRestore() : old_action_(), signo_(-1), valid_(false) {}

  ~ScopedSigactionRestore() { Reset(); }

  bool Reset() {
    if (valid_) {
      int res = sigaction(signo_, &old_action_, nullptr);
      EXPECT_EQ(res, 0) << ErrnoMessage("sigaction");
      if (res != 0) {
        return false;
      }
    }
    valid_ = false;
    signo_ = -1;
    return true;
  }

  bool ResetInstallHandler(int signo, Signals::Handler handler) {
    if (Reset() && Signals::InstallHandler(signo, handler, 0, &old_action_)) {
      signo_ = signo;
      valid_ = true;
      return true;
    }
    return false;
  }

 private:
  struct sigaction old_action_;
  int signo_;
  bool valid_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSigactionRestore);
};

class RaiseTest {
 public:
  static void Run() {
    test_complete_ = false;

    ScopedSigactionRestore sigrestore;
    ASSERT_TRUE(sigrestore.ResetInstallHandler(kSigno, HandleRaisedSignal));

    EXPECT_EQ(raise(kSigno), 0) << ErrnoMessage("raise");
    EXPECT_TRUE(test_complete_);
  }

 private:
  static void HandleRaisedSignal(int signo, siginfo_t* siginfo, void* context) {
    FakePtraceConnection connection;
    ASSERT_TRUE(connection.Initialize(getpid()));

    ProcessReader process_reader;
    ASSERT_TRUE(process_reader.Initialize(&connection));

    internal::ExceptionSnapshotLinux exception;
    ASSERT_TRUE(exception.Initialize(&process_reader,
                                     FromPointerCast<LinuxVMAddress>(siginfo),
                                     FromPointerCast<LinuxVMAddress>(context),
                                     gettid()));

    EXPECT_EQ(exception.Exception(), static_cast<uint32_t>(kSigno));

    EXPECT_EQ(exception.Codes().size(), 3u);
    EXPECT_EQ(exception.Codes()[0], static_cast<uint64_t>(getpid()));
    EXPECT_EQ(exception.Codes()[1], getuid());
    // Codes()[2] is not set by kill, but we still expect to get it because some
    // interfaces may set it and we don't necessarily know where this signal
    // came
    // from.

    test_complete_ = true;
  }

  static constexpr uint32_t kSigno = SIGUSR1;
  static bool test_complete_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(RaiseTest);
};
bool RaiseTest::test_complete_ = false;

TEST(ExceptionSnapshotLinux, Raise) {
  RaiseTest::Run();
}

class TimerTest {
 public:
  TimerTest() : event_(), timer_(-1), test_complete_(0) { test_ = this; }
  ~TimerTest() { test_ = nullptr; }

  void Run() {
    ScopedSigactionRestore sigrestore;
    ASSERT_TRUE(sigrestore.ResetInstallHandler(kSigno, HandleTimer));

    event_.sigev_notify = SIGEV_SIGNAL;
    event_.sigev_signo = kSigno;
    event_.sigev_value.sival_int = 42;
    ASSERT_EQ(syscall(SYS_timer_create, CLOCK_MONOTONIC, &event_, &timer_), 0);

    itimerspec spec;
    spec.it_interval.tv_sec = 0;
    spec.it_interval.tv_nsec = 0;
    spec.it_value.tv_sec = 0;
    spec.it_value.tv_nsec = 1;
    ASSERT_EQ(syscall(SYS_timer_settime, timer_, TIMER_ABSTIME, &spec, nullptr),
              0);

    ASSERT_TRUE(test_complete_.TimedWait(5));
  }

 private:
  static void HandleTimer(int signo, siginfo_t* siginfo, void* context) {
    FakePtraceConnection connection;
    ASSERT_TRUE(connection.Initialize(getpid()));

    ProcessReader process_reader;
    ASSERT_TRUE(process_reader.Initialize(&connection));

    internal::ExceptionSnapshotLinux exception;
    ASSERT_TRUE(exception.Initialize(&process_reader,
                                     FromPointerCast<LinuxVMAddress>(siginfo),
                                     FromPointerCast<LinuxVMAddress>(context),
                                     gettid()));

    EXPECT_EQ(exception.Exception(), static_cast<uint32_t>(kSigno));

    EXPECT_EQ(exception.Codes().size(), 3u);
    EXPECT_EQ(exception.Codes()[0], static_cast<uint64_t>(test_->timer_));
    int overruns = syscall(SYS_timer_getoverrun, test_->timer_);
    ASSERT_GE(overruns, 0);
    EXPECT_EQ(exception.Codes()[1], static_cast<uint64_t>(overruns));
    EXPECT_EQ(exception.Codes()[2],
              static_cast<uint64_t>(test_->event_.sigev_value.sival_int));

    test_->test_complete_.Signal();
  }

  sigevent event_;
  __kernel_timer_t timer_;
  Semaphore test_complete_;

  static constexpr uint32_t kSigno = SIGALRM;
  static TimerTest* test_;

  DISALLOW_COPY_AND_ASSIGN(TimerTest);
};
TimerTest* TimerTest::test_;

TEST(ExceptionSnapshotLinux, SelfTimer) {
  TimerTest test;
  test.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
