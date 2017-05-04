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

#include "util/linux/ptrace_requests.h"

#include <elf.h>
#include <sys/ptrace.h>
#include <sys/uio.h>

#include "base/logging.h"

namespace crashpad {

// PTRACE_GETREGSET, introduced in Linux 2.6.34 (2225a122ae26),
// requires kernel support enabled by HAVE_ARCH_TRACEHOOK. This has
// been set for x86 (including x86_64) since Linux 2.6.28
// (99bbc4b1e677a), but for ARM only since Linux 3.5.0
// (0693bf68148c4). Fortunately, 64-bit ARM support only appeared in
// Linux 3.7.0, so if PTRACE_GETREGSET fails on ARM with EIO,
// indicating that the request is not supported, the kernel must be
// old enough that 64-bit ARM isn’t supported either.
// TODO(jperaza) handle this.
GetRegistersResult GetGeneralPurposeRegisters(pid_t pid,
                                              ThreadContext* context) {
  iovec iov;
  iov.iov_base = context;
  iov.iov_len = sizeof(*context);
  if (ptrace(PTRACE_GETREGSET, pid, reinterpret_cast<void*>(NT_PRSTATUS), &iov) != 0) {
    PLOG(ERROR) << "ptrace";
    return GetRegistersResult::kError;
  }
  if (iov.iov_len == sizeof(context->t64)) {
    return GetRegistersResult::k64Bit;
  } else if (iov.iov_len == sizeof(context->t32)) {
    return GetRegistersResult::k32Bit;
  } else {
    return GetRegistersResult::kError;
  }
}

// ARM regsets
//   REGSET_GPR
//   REGSET_FPR
//   REGSET_VFP if CONFIG_VFP
//
// ARM64 regsets
//   REGSET_GPR
//   REGSET_FPR
//   REGSET_TLS
//
//   compat mode:
//     REGSET_COMPAT_GPR
//     REGSET_COMPAT_VFP
GetRegistersResult GetFloatingPointRegisters(pid_t pid, FloatContext* context) {
#if defined (ARCH_CPU_X86_FAMILY)
  iovec iov;
  iov.iov_base = context;
  iov.iov_len = sizeof(*context);
  if (ptrace(PTRACE_GETREGSET, pid, reinterpret_cast<void*>(NT_PRFPREG), &iov) != 0) {
    PLOG(ERROR) << "ptrace";
    return GetRegistersResult::kError;
  }
  if (iov.iov_len == sizeof(context->f64)) {
    return GetRegistersResult::k64Bit;
  } else if (iov.iov_len == sizeof(context->f32)) {
    return GetRegistersResult::k32Bit;
  } else {
    return GetRegistersResult::kError;
  }
#elif defined(ARCH_CPU_ARM_FAMILY)
  // 64-bit processes should only have registers collected with NT_PRFPREG
  // 32-bit process may have one or two register sets collected with NT_PRFPREG
  // and NT_ARM_VFP, depending on configuration.
  iovec iov;
  iov.iov_base = context;
  iov.iov_len = sizeof(*context);
  if (ptrace(PTRACE_GETREGSET, pid, reinterpret_cast<void*>(NT_PRFPREG), &iov) != 0) {
    switch (errno) {
      case EIO:
        // GETREGSET not supported on older linux for ARM
        // TODO(jperaza): use GETFPREGS instead
        break;
      case EINVAL:
        // ARM on ARM64 cpu doesn't have this register set
        // It should have a VFP register set
        break;
      default:
        PLOG(ERROR) << "ptrace";
        return GetRegistersResult::kError;
    }
  } else {
    if (iov.iov_len == FPSIMD_SIZE) {
      // Target process is 64-bit; there are no other registers sets.
      return GetRegistersResult::k64Bit;
    } else if (iov.iov_len == FPREGS_SIZE) {
      // Target process and the cpu are 32-bit; there might be VFP registers.
      context->f32.have_fpregs = true;
    } else {
      LOG(ERROR) << "Unexpected registers size";
      return GetRegistersResult::kError;
    }
  }

  // Target process is 32-bit; there may be VFP registers.
  iov.iov_base = &context->f32.vfp;
  iov.iov_len = sizeof(context->f32.vfp);
  if (ptrace(PTRACE_GETREGSET, pid, reinterpret_cast<void*>(NT_ARM_VFP), &iov) != 0) {
    switch (errno) {
      case EIO:
        // GETREGSET not supported on older linux for ARM
        // TODO(jperaza): use GETVFPREGS instead
        break;
      case EINVAL:
        // VFP is optional on 32-bit ARM cpus
        break;
      default:
        PLOG(ERROR) << "ptrace";
        return GetRegistersResult::kError;
    }
  } else if (iov.iov_len == VFP_SIZE) {
    context->f32.have_vfp = true;
  } else {
    LOG(ERROR) << "Unexpected registers size";
    return GetRegistersResult::kError;
  }

  if (!(context->f32.have_fpregs || context->f32.have_vfp)) {
    LOG(ERROR) << "Unable to collect registers";
    return GetRegistersResult::kError;
  }
  return GetRegistersResult::k32Bit;
#else
#error Port.
#endif  // ARCH_CPU_X86_FAMILY
}

}  // namespace crashpad
