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

//! \brief The set of general purpose registers for an architecture family.
union ThreadContext {
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
#define FPSIMD_SIZE (32 * 16 + 2 * 4 + 8)
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
    // Are the fpr and vfp separate?
    // Can you have both?
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
#define FPREGS_SIZE (11 * 4 + 8)
    struct vfp {
      uint64_t fpregs[32];
      uint32_t fpscr;
    } vfp;
#define VFP_SIZE (32 * 8 + 4)
    bool have_fpregs;
    bool have_vfp;
#else
#error Port.
#endif  // ARCH_CPU_X86_FAMILY
  } f32;
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
              "ThreadContext is not standard layout");

#if defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARM64)
#define USER_GPR_STRUCT user_regs_struct
#elif defined(ARCH_CPU_ARMEL)
#define USER_GPR_STRUCT user_regs
#else
#error Port.
#endif

static_assert(std::is_standard_layout<USER_GPR_STRUCT>::value,
              "USER_GPR_STRUCT is not standard layout");

#if defined(ARCH_CPU_64_BITS)
#define GPR_STRUCT ThreadContext::t64
#else
#define GPR_STRUCT ThreadContext::t32
#endif

static_assert(sizeof(GPR_STRUCT) == sizeof(USER_GPR_STRUCT),
              "Incorrect gpr struct size");
#undef GPR_STRUCT
#undef USER_GPR_STRUCT

// FloatContext

static_assert(std::is_standard_layout<FloatContext>::value,
              "FloatContext is not standard layout");

#if defined(ARCH_CPU_X86_FAMILY)
static_assert(std::is_standard_layout<user_fpregs_struct>::value,
              "user_fpregs_struct is not standard layout");
#elif defined(ARCH_CPU_ARM64)
static_assert(std::is_standard_layout<user_fpsimd_struct>::value,
              "user_fpsimd_struct is not standard layout");
static_assert(sizeof(FloatContext::f64) == sizeof(user_fpsimd_struct),
              "Incorrect fpr size");
#elif defined(ARCH_CPU_ARMEL)
static_assert(std::is_standard_layout<user_fpregs>::value,
              "user_fpregs is not standard layout");
static_assert(sizeof(FloatContext::f32::fpregs) == sizeof(user_fpregs),
              "fpregs is wrong size");
static_assert(std::is_standard_layout<user_vfp>::value,
              "user_vfp is not standard layout");
static_assert(sizeof(FloatContext::f32::vfp) == sizeof(user_vfp),
              "vfp is wrong size");
#else
#error Port.
#endif

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_LINUX_PTRACE_REQUESTS_H_
