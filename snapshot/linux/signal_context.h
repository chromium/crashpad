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

namespace crashpad {
namespace internal {

using Nothing = char[0];

struct Traits32 {
  using Address = uint32_t;
  using Long = int32_t;
  using ULong = uint32_t;
  using Clock = Long;
  using Size = uint32_t;
  using ULong_32Only = ULong;
  using ULong_64Only = Nothing;
};

struct Traits64 {
  using Address = uint64_t;
  using Long = int64_t;
  using ULong = uint64_t;
  using Clock = Long;
  using Size = uint64_t;
  using ULong_32Only = Nothing;
  using ULong_64Only = ULong;
};

template <class Traits>
struct Sigval {
  int32_t sigval;
  typename Traits::Address pointer;
};

template <class Traits>
struct Siginfo {
  int32_t signo;
  int32_t errno;
  int32_t code;

  union {
    struct {
      pid_t pid;
      uid_t uid;
      union {
        Sigval<Traits> sigval;
        struct {
          int32_t status;
          typename Traits::Clock utime;
          typename Traits::Clock stime;
        };
      };
    };

    struct {
      int32_t timerid;
      int32_t overrun;
    };

    struct {
      typename Traits::Address address;
    };

    struct {
      typename Traits::Long band;
      int32_t fd;
    };

    struct {
      typename Traits::Address call_address;
      int32_t syscall;
      uint32_t arch;
    };
  };
};

struct SignalThreadContext32 {
  uint32_t gs;
  uint32_t fs;
  uint32_t es;
  uint32_t ds;
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
  uint32_t cs;
  uint32_t efl;
  uint32_t uesp;
  uint32_t ss;
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
  uint64_t efl;
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
  uint32_t status;
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
  typename Traits::Size size;
};

template <typename Traits>
struct UContext {
  typename Traits::ULong flags;
  typename Traits::Address link;
  SignalStack<Traits> stack;
  MContext<Traits> mcontext;
  uint64_t sigmask;
  typename Traits::FloatContext fprs;
};

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_LINUX_SNAPSHOT_SIGNAL_CONTEXT_LINUX_H_
