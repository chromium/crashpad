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

#ifndef CRASHPAD_SNAPSHOT_LINUX_SNAPSHOT_SIGNAL_CONTEXT_LINUX_H_
#define CRASHPAD_SNAPSHOT_LINUX_SNAPSHOT_SIGNAL_CONTEXT_LINUX_H_

#include <stdint.h>
#include <sys/types.h>

#include "build/build_config.h"
#include "util/linux/traits.h"

namespace crashpad {
namespace internal {

#pragma pack(push, 1)

template <class Traits>
union Sigval {
  int32_t sigval;
  typename Traits::Address pointer;
};

template <class Traits>
struct Siginfo {
  int32_t signo;
  int32_t err;
  int32_t code;
  typename Traits::UInteger32_64Only padding;

  union {
    // SIGSEGV, SIGILL, SIGFPE, SIGBUS, SIGTRAP
    struct {
      typename Traits::Address address;
    };

    // SIGPOLL
    struct {
      typename Traits::Long band;
      int32_t fd;
    };

    // SIGSYS
    struct {
      typename Traits::Address call_address;
      int32_t syscall;
      uint32_t arch;
    };

    // Everything else
    struct {
      union {
        struct {
          pid_t pid;
          uid_t uid;
        };
        struct {
          int32_t timerid;
          int32_t overrun;
        };
      };

      union {
        Sigval<Traits> sigval;

        // SIGCHLD
        struct {
          int32_t status;
          typename Traits::Clock utime;
          typename Traits::Clock stime;
        };
      };
    };
  };
};

#if defined(ARCH_CPU_X86_FAMILY)

struct SignalThreadContext32 {
  uint32_t xgs;
  uint32_t xfs;
  uint32_t xes;
  uint32_t xds;
  uint32_t edi;
  uint32_t esi;
  uint32_t ebp;
  uint32_t esp;
  uint32_t ebx;
  uint32_t edx;
  uint32_t ecx;
  uint32_t eax;
  uint32_t trapno;
  uint32_t err;
  uint32_t eip;
  uint32_t xcs;
  uint32_t eflags;
  uint32_t uesp;
  uint32_t xss;
};

struct SignalThreadContext64 {
  uint64_t r8;
  uint64_t r9;
  uint64_t r10;
  uint64_t r11;
  uint64_t r12;
  uint64_t r13;
  uint64_t r14;
  uint64_t r15;
  uint64_t rdi;
  uint64_t rsi;
  uint64_t rbp;
  uint64_t rbx;
  uint64_t rdx;
  uint64_t rax;
  uint64_t rcx;
  uint64_t rsp;
  uint64_t rip;
  uint64_t eflags;
  uint16_t cs;
  uint16_t gs;
  uint16_t fs;
  uint16_t padding;
  uint64_t err;
  uint64_t trapno;
  uint64_t oldmask;
  uint64_t cr2;
};

struct SignalFloatContext32 {
  CPUContextX86::Fsave fsave;
  uint16_t status;
  uint16_t magic;
  CPUContextX86::Fxsave fxsave[0];
};

using SignalFloatContext64 = CPUContextX86_64::Fxsave;

struct ContextTraits32 : public Traits32 {
  using ThreadContext = SignalThreadContext32;
  using FloatContext = SignalFloatContext32;
};

struct ContextTraits64 : public Traits64 {
  using ThreadContext = SignalThreadContext64;
  using FloatContext = SignalFloatContext64;
};

template <typename Traits>
struct MContext {
  typename Traits::ThreadContext gprs;
  typename Traits::Address fpptr;
  typename Traits::ULong_32Only oldmask;
  typename Traits::ULong_32Only cr2;
  typename Traits::ULong_64Only reserved[8];
};

template <typename Traits>
struct SignalStack {
  typename Traits::Address stack_pointer;
  uint32_t flags;
  typename Traits::UInteger32_64Only padding;
  typename Traits::Size size;
};

template <typename Traits>
struct Sigset {};

template <>
struct Sigset<ContextTraits32> {
  uint64_t val;
};

#if defined(OS_ANDROID)
template <>
struct Sigset<ContextTraits64> {
  uint64_t val;
};
#else
template <>
struct Sigset<ContextTraits64> {
  ContextTraits64::ULong val[16];
};
#endif  // OS_ANDROID

template <typename Traits>
struct UContext {
  typename Traits::ULong flags;
  typename Traits::Address link;
  SignalStack<Traits> stack;
  MContext<Traits> mcontext;
  Sigset<Traits> sigmask;
  typename Traits::FloatContext fprs;
};

#else
#error Port.
#endif  // ARCH_CPU_X86_FAMILY

#pragma pack(pop)

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_LINUX_SNAPSHOT_SIGNAL_CONTEXT_LINUX_H_
