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

#include "util/posix/signals.h"

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/ucontext.h>
#include <unistd.h>

#include <limits>

#include "base/compiler_specific.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "test/errors.h"
#include "test/multiprocess.h"
#include "test/scoped_temp_dir.h"
#include "util/posix/scoped_mmap.h"

namespace crashpad {
namespace test {
namespace {

int g_advance;
bool g_didHandle;

constexpr int kUnexpectedExitStatus = 3;

// Keep synchronized with CauseSignal().
bool CanCauseSignal(int sig) {
  return sig == SIGABRT || sig == SIGALRM || sig == SIGBUS ||
#if !defined(ARCH_CPU_ARM64)
         sig == SIGFPE ||
#endif  // !defined(ARCH_CPU_ARM64)
#if defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARMEL)
         sig == SIGILL ||
#endif  // defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARMEL
         sig == SIGPIPE || sig == SIGSEGV ||
#if defined(OS_MACOSX)
         sig == SIGSYS ||
#endif  // OS_MACOSX
#if defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARM64)
         sig == SIGTRAP ||
#endif  // defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARM64)
         false;
}

// Keep synchronized with CanCauseSignal().
void CauseSignal(int sig) {
  switch (sig) {
    case SIGABRT: {
      g_advance = 1;
      raise(SIGABRT);
      break;
    }

    case SIGALRM: {
      g_advance = 0;
      struct itimerval itimer = {};
      itimer.it_value.tv_usec = 1E3;  // 1 millisecond
      if (setitimer(ITIMER_REAL, &itimer, nullptr) != 0) {
        PLOG(ERROR) << "setitimer";
        _exit(kUnexpectedExitStatus);
      }
      sleep(2);
      break;
    }

    case SIGBUS: {
      g_advance = 3;
      ScopedMmap mapped_file;
      {
        base::ScopedFD fd;
        {
          ScopedTempDir temp_dir;
          fd.reset(open(temp_dir.path().Append("empty").value().c_str(),
                        O_RDWR | O_CREAT | O_EXCL | O_NOCTTY | O_CLOEXEC,
                        0644));
          if (fd.get() < 0) {
            PLOG(ERROR) << "open";
          }
        }
        if (fd.get() < 0) {
          _exit(kUnexpectedExitStatus);
        }

        if (!mapped_file.ResetMmap(nullptr,
                                   getpagesize(),
                                   PROT_READ | PROT_WRITE,
                                   MAP_PRIVATE,
                                   fd.get(),
                                   0)) {
          _exit(kUnexpectedExitStatus);
        }
      }

      *mapped_file.addr_as<char*>() = 0;
      break;
    }

#if !defined(ARCH_CPU_ARM64)
    // ARM64 has hardware integer division instructions that don’t generate a
    // trap for divide-by-zero, so this doesn’t produce SIGFPE.
    case SIGFPE: {
      g_advance = 2;
      // Optimization makes this tricky, so get zero from a system call likely
      // to succeed, and try to do something with the result.
      struct stat stat_buf;
      int zero = stat("/", &stat_buf);
      if (zero == -1) {
        // It’s important to check |== -1| and not |!= 0|. An optimizer is free
        // to discard an |== 0| branch entirely, because division by zero is
        // undefined behavior.
        PLOG(ERROR) << "stat";
        _exit(kUnexpectedExitStatus);
      }

      int quotient = 2 / zero;
      fstat(quotient, &stat_buf);
      break;
    }
#endif  // ARCH_CPU_ARM64

#if defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARMEL)
    case SIGILL: {
      // __builtin_trap() causes SIGTRAP on arm64 on Android.
      g_advance = 2;
      __builtin_trap();
      break;
    }
#endif  // defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARMEL)

    case SIGPIPE: {
      g_advance = 0;
      int pipe_fds[2];
      if (pipe(pipe_fds) != 0) {
        PLOG(ERROR) << "pipe";
        _exit(kUnexpectedExitStatus);
      }

      if (close(pipe_fds[0]) != 0) {
        PLOG(ERROR) << "close";
        _exit(kUnexpectedExitStatus);
      }

      char c = 0;
      write(pipe_fds[1], &c, sizeof(c));
      break;
    }

    case SIGSEGV: {
      g_advance = 3;
      volatile int* i = nullptr;
      *i = 0;
      break;
    }

#if defined(OS_MACOSX)
    case SIGSYS: {
      g_advance = 4;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
      syscall(4095);
#pragma clang diagnostic pop
      break;
    }
#endif  // OS_MACOSX

#if defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARM64)
    case SIGTRAP: {
#if defined(ARCH_CPU_X86_FAMILY)
      g_advance = 0;
      asm("int3");
#elif defined(ARCH_CPU_ARM64)
      // bkpt #0 should work for 32-bit ARCH_CPU_ARMEL, but according to
      // https://crrev.com/f53167270c44, it only causes SIGTRAP on Linux under a
      // 64-bit kernel. For a pure 32-bit armv7 system, it generates SIGBUS.
      asm("brk #0");
#endif
      break;
    }
#endif  // defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARM64)

    default: {
      LOG(ERROR) << "unexpected signal " << sig;
      _exit(kUnexpectedExitStatus);
      break;
    }
  }
}

class SignalsiOSTest {
 public:
  SignalsiOSTest(int sig) : sig_(sig) {}
  ~SignalsiOSTest() {}

  void Run() {
    bool (*install_handlers)(Signals::Handler, int, Signals::OldActions*);
    if (Signals::IsCrashSignal(sig_)) {
      install_handlers = [](Signals::Handler handler,
                            int sig,
                            Signals::OldActions* old_actions) {
        return Signals::InstallCrashHandlers(
            handler, sig, old_actions, nullptr);
      };
    } else if (Signals::IsTerminateSignal(sig_)) {
      install_handlers = Signals::InstallTerminateHandlers;
    } else {
      _exit(kUnexpectedExitStatus);
    }

    ASSERT_TRUE(install_handlers(SignalHandler_FixUps, 0, nullptr));
    CauseSignal(sig_);
  }

 private:
  static void SignalHandler_FixUps(int sig, siginfo_t* siginfo, void* context) {
    g_didHandle = true;
    if (g_advance > 0)
      ((ucontext_t*)context)->uc_mcontext->__ss.__rip += g_advance;
  }

  int sig_;
  DISALLOW_COPY_AND_ASSIGN(SignalsiOSTest);
};

TEST(SignalsiOS, WillSignalReraiseAutonomously) {
  const struct {
    int sig;
    int code;
    bool result;
  } kTestData[] = {
      {SIGBUS, BUS_ADRALN, true},
      {SIGFPE, FPE_FLTDIV, true},
      {SIGILL, ILL_ILLOPC, true},
      {SIGSEGV, SEGV_MAPERR, true},
      {SIGBUS, 0, false},
      {SIGFPE, -1, false},
      {SIGILL, SI_USER, false},
      {SIGSEGV, SI_QUEUE, false},
      {SIGTRAP, TRAP_BRKPT, false},
      {SIGHUP, SEGV_MAPERR, false},
      {SIGINT, SI_USER, false},
  };
  for (size_t index = 0; index < base::size(kTestData); ++index) {
    const auto test_data = kTestData[index];
    SCOPED_TRACE(base::StringPrintf(
        "index %zu, sig %d, code %d", index, test_data.sig, test_data.code));
    siginfo_t siginfo = {};
    siginfo.si_signo = test_data.sig;
    siginfo.si_code = test_data.code;
    EXPECT_EQ(Signals::WillSignalReraiseAutonomously(&siginfo),
              test_data.result);
  }
}

TEST(SignalsiOS, IsCrashSignal) {
  // Always crash signals.
  EXPECT_TRUE(Signals::IsCrashSignal(SIGABRT));
  EXPECT_TRUE(Signals::IsCrashSignal(SIGBUS));
  EXPECT_TRUE(Signals::IsCrashSignal(SIGFPE));
  EXPECT_TRUE(Signals::IsCrashSignal(SIGILL));
  EXPECT_TRUE(Signals::IsCrashSignal(SIGQUIT));
  EXPECT_TRUE(Signals::IsCrashSignal(SIGSEGV));
  EXPECT_TRUE(Signals::IsCrashSignal(SIGSYS));
  EXPECT_TRUE(Signals::IsCrashSignal(SIGTRAP));

  // Always terminate signals.
  EXPECT_FALSE(Signals::IsCrashSignal(SIGALRM));
  EXPECT_FALSE(Signals::IsCrashSignal(SIGHUP));
  EXPECT_FALSE(Signals::IsCrashSignal(SIGINT));
  EXPECT_FALSE(Signals::IsCrashSignal(SIGPIPE));
  EXPECT_FALSE(Signals::IsCrashSignal(SIGPROF));
  EXPECT_FALSE(Signals::IsCrashSignal(SIGTERM));
  EXPECT_FALSE(Signals::IsCrashSignal(SIGUSR1));
  EXPECT_FALSE(Signals::IsCrashSignal(SIGUSR2));
  EXPECT_FALSE(Signals::IsCrashSignal(SIGVTALRM));

  // Never crash or terminate signals.
  EXPECT_FALSE(Signals::IsCrashSignal(SIGCHLD));
  EXPECT_FALSE(Signals::IsCrashSignal(SIGCONT));
  EXPECT_FALSE(Signals::IsCrashSignal(SIGTSTP));
  EXPECT_FALSE(Signals::IsCrashSignal(SIGTTIN));
  EXPECT_FALSE(Signals::IsCrashSignal(SIGTTOU));
  EXPECT_FALSE(Signals::IsCrashSignal(SIGURG));
  EXPECT_FALSE(Signals::IsCrashSignal(SIGWINCH));
}

TEST(SignalsiOS, Cause_HandlerContinues) {
  for (int sig = 1; sig < NSIG; ++sig) {
    SCOPED_TRACE(base::StringPrintf("sig %d (%s)", sig, strsignal(sig)));

    if (!CanCauseSignal(sig)) {
      continue;
    }

    SignalsiOSTest test(sig);
    g_didHandle = false;
    test.Run();
    EXPECT_TRUE(g_didHandle);
  }
}

TEST(SignalsiOS, IsTerminateSignal) {
  // Always terminate signals.
  EXPECT_TRUE(Signals::IsTerminateSignal(SIGALRM));
  EXPECT_TRUE(Signals::IsTerminateSignal(SIGHUP));
  EXPECT_TRUE(Signals::IsTerminateSignal(SIGINT));
  EXPECT_TRUE(Signals::IsTerminateSignal(SIGPIPE));
  EXPECT_TRUE(Signals::IsTerminateSignal(SIGPROF));
  EXPECT_TRUE(Signals::IsTerminateSignal(SIGTERM));
  EXPECT_TRUE(Signals::IsTerminateSignal(SIGUSR1));
  EXPECT_TRUE(Signals::IsTerminateSignal(SIGUSR2));
  EXPECT_TRUE(Signals::IsTerminateSignal(SIGVTALRM));

  // Always crash signals.
  EXPECT_FALSE(Signals::IsTerminateSignal(SIGABRT));
  EXPECT_FALSE(Signals::IsTerminateSignal(SIGBUS));
  EXPECT_FALSE(Signals::IsTerminateSignal(SIGFPE));
  EXPECT_FALSE(Signals::IsTerminateSignal(SIGILL));
  EXPECT_FALSE(Signals::IsTerminateSignal(SIGQUIT));
  EXPECT_FALSE(Signals::IsTerminateSignal(SIGSEGV));
  EXPECT_FALSE(Signals::IsTerminateSignal(SIGSYS));
  EXPECT_FALSE(Signals::IsTerminateSignal(SIGTRAP));

  // Never crash or terminate signals.
  EXPECT_FALSE(Signals::IsTerminateSignal(SIGCHLD));
  EXPECT_FALSE(Signals::IsTerminateSignal(SIGCONT));
  EXPECT_FALSE(Signals::IsTerminateSignal(SIGTSTP));
  EXPECT_FALSE(Signals::IsTerminateSignal(SIGTTIN));
  EXPECT_FALSE(Signals::IsTerminateSignal(SIGTTOU));
  EXPECT_FALSE(Signals::IsTerminateSignal(SIGURG));
  EXPECT_FALSE(Signals::IsTerminateSignal(SIGWINCH));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
