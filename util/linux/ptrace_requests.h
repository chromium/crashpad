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

#ifndef CRASHPAD_UTIL_LINUX_PTRACE_REQUESTS_H_
#define CRASHPAD_UTIL_LINUX_PTRACE_REQUESTS_H_

#include <stdint.h>
#include <sys/types.h>
#include <sys/user.h>

#include <type_traits>

#include "build/build_config.h"
#include "util/numeric/int128.h"

namespace crashpad {

//! \brief The set of general purpose registers for an architecture family.
union ThreadContext {
  ThreadContext();
  ~ThreadContext();

  //! \brief The general purpose registers used by the 64-bit variant of the
  //!     architecture.
  struct t64 {
#if defined(ARCH_CPU_X86_FAMILY)
    uint64_t r15;
    uint64_t r14;
    uint64_t r13;
    uint64_t r12;
    uint64_t rbp;
    uint64_t rbx;
    uint64_t r11;
    uint64_t r10;
    uint64_t r9;
    uint64_t r8;
    uint64_t rax;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t orig_rax;
    uint64_t rip;
    uint64_t cs;
    uint64_t eflags;
    uint64_t rsp;
    uint64_t ss;
    uint64_t fs_base;
    uint64_t gs_base;
    uint64_t ds;
    uint64_t es;
    uint64_t fs;
    uint64_t gs;
#elif defined(ARCH_CPU_ARM_FAMILY)
    uint64_t regs[31];
    uint64_t sp;
    uint64_t pc;
    uint64_t pstate;
#else
#error Port.
#endif  // ARCH_CPU_X86_FAMILY
  } t64;

  //! \brief The general purpose registers used by the 32-bit variant of the
  //!     architecture.
  struct t32 {
#if defined(ARCH_CPU_X86_FAMILY)
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
    uint32_t eax;
    uint32_t xds;
    uint32_t xes;
    uint32_t xfs;
    uint32_t xgs;
    uint32_t orig_eax;
    uint32_t eip;
    uint32_t xcs;
    uint32_t eflags;
    uint32_t esp;
    uint32_t xss;
#elif defined(ARCH_CPU_ARM_FAMILY)
    uint32_t regs[18];
#else
#error Port.
#endif  // ARCH_CPU_X86_FAMILY
  } t32;
};

//! \brief The floating point registers used for an architecture family.
union FloatContext {
  FloatContext();
  ~FloatContext();

  //! \brief The floating point registers used by the 64-bit variant of the
  //!     architecture.
  struct f64 {
#if defined(ARCH_CPU_X86_FAMILY)
    uint16_t cwd;
    uint16_t swd;
    uint16_t ftw;
    uint16_t fop;
    uint64_t rip;
    uint64_t rdp;
    uint32_t mxcsr;
    uint32_t mxcr_mask;
    uint32_t st_space[32];
    uint32_t xmm_space[64];
    uint32_t padding[24];
#elif defined(ARCH_CPU_ARM_FAMILY)
    uint128_struct vregs[32];
    uint32_t fpsr;
    uint32_t fpcr;
    uint8_t padding[8];
#else
#error Port.
#endif  // ARCH_CPU_X86_FAMILY
  } f64;

  //! \brief The floating point registers used by the 32-bit variant of the
  //!     architecture.
  struct f32 {
#if defined(ARCH_CPU_X86_FAMILY)
    uint32_t cwd;
    uint32_t swd;
    uint32_t twd;
    uint32_t fip;
    uint32_t fcs;
    uint32_t foo;
    uint32_t fos;
    uint32_t st_space[20];
#elif defined(ARCH_CPU_ARM_FAMILY)
    struct fpregs {
      struct fp_reg {
        uint32_t sign1 : 1;
        uint32_t unused : 15;
        uint32_t sign2 : 1;
        uint32_t exponent : 14;
        uint32_t j : 1;
        uint32_t mantissa1 : 31;
        uint32_t mantisss0 : 32;
      } fpregs[8];
      uint32_t fpsr : 32;
      uint32_t fpcr : 32;
      uint8_t type[8];
      uint32_t init_flag;
    } fpregs;
    struct vfp {
      uint64_t fpregs[32];
      uint32_t fpscr;
    } vfp;
    bool have_fpregs;
    bool have_vfp;
#else
#error Port.
#endif  // ARCH_CPU_X86_FAMILY
  } f32;
};

//! \brief Indicates which field of a ThreadContext or FloatContext union is
//!     active.
enum class GetRegistersResult {
  //! \brief An error occurred and no field of the union is valid.
  kError = 0,

  //! \brief The field for the 64-bit variant is active.
  k64Bit,

  //! \brief The field for the 32-bit variant is active.
  k32Bit
};

//! \brief Uses `ptrace` to collect general purpose registers from the process
//!     whose process ID is \a pid and places the result in \a context.
//!
//! The target process must be ptrace-attached before calling this function.
//!
//! \param[in] pid The process ID to collect registers values for.
//! \param[out] context The registers read from the target process.
//!
//! \return A GetRegistersResult indicating which field in \a context is active.
GetRegistersResult GetGeneralPurposeRegisters(pid_t pid,
                                              ThreadContext* context);

//! \brief Uses `ptrace` to collect floating point registers from the process
//!     whose process ID is \a pid and places the result in \a context.
//!
//! The target process must be ptrace-attached before calling this function.
//!
//! \param[in] pid The process ID to collect registers values for.
//! \param[out] context The registers read from the target process.
//!
//! \return A GetRegistersResult indicating which field in \a context is active.
GetRegistersResult GetFloatingPointRegisters(pid_t pid, FloatContext* context);

// The structs contained in ThreadContext and FloatContext should be
// interoperable with the structs defined in sys/user.h. These assertions
// help verify that they maintain binary compatibility.
static_assert(std::is_standard_layout<ThreadContext>::value,
              "Not standard layout");
static_assert(std::is_standard_layout<FloatContext>::value,
              "Not standard layout");

#if defined(ARCH_CPU_ARMEL)
static_assert(sizeof(ThreadContext::t32) == sizeof(user_regs), "Size mismatch");
#elif defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARM64)
#if defined(ARCH_CPU_32_BITS)
static_assert(sizeof(ThreadContext::t32) == sizeof(user_regs_struct),
              "Size mismatch");
#else  // ARCH_CPU_64_BITS
static_assert(sizeof(ThreadContext::t64) == sizeof(user_regs_struct),
              "Size mismatch");
#endif  // ARCH_CPU_32_BITS
#else
#error Port.
#endif  // ARCH_CPU_ARMEL

#if defined(ARCH_CPU_ARMEL)
static_assert(sizeof(FloatContext::f32::fpregs) == sizeof(user_fpregs),
              "Size mismatch");
static_assert(sizeof(FloatContext::f32::vfp) == sizeof(user_vfp),
              "Size mismatch");
#elif defined(ARCH_CPU_ARM64)
static_assert(sizeof(FloatContext::f64) == sizeof(user_fpsimd_struct),
              "Size mismatch");
#elif defined(ARCH_CPU_X86)
static_assert(sizeof(FloatContext::f32) == sizeof(user_fpregs_struct),
              "Size mismatch");
#elif defined(ARCH_CPU_X86_64)
static_assert(sizeof(FloatContext::f64) == sizeof(user_fpregs_struct),
              "Size mismatch");
#else
#error Port.
#endif  // ARCH_CPU_X86_64

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_LINUX_PTRACE_REQUESTS_H_
