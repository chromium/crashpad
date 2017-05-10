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
#include <string.h>
#include <sys/ptrace.h>
#include <sys/uio.h>

#include "base/logging.h"
#include "util/misc/from_pointer_cast.h"

#if defined(ARCH_CPU_X86_FAMILY)
#include <asm/ldt.h>
#include <asm/prctl.h>
#endif

namespace crashpad {

namespace {

#if defined(ARCH_CPU_ARMEL)
// PTRACE_GETREGSET, introduced in Linux 2.6.34 (2225a122ae26), requires kernel
// support enabled by HAVE_ARCH_TRACEHOOK. This has been set for x86 (including
// x86_64) since Linux 2.6.28 (99bbc4b1e677a), but for ARM only since
// Linux 3.5.0 (0693bf68148c4). Older Linux kernels support PTRACE_GETREGS,
// PTRACE_GETFPREGS, and PTRACE_GETVFPREGS instead, which don't allow checking
// the size of data copied.
//
// Fortunately, 64-bit ARM support only appeared in Linux 3.7.0, so if
// PTRACE_GETREGSET fails on ARM with EIO, indicating that the request is not
// supported, the kernel must be old enough that 64-bit ARM isn’t supported
// either.
//
// TODO(mark): Once helpers to interpret the kernel version are available, add
// a DCHECK to ensure that the kernel is older than 3.5.

GetRegistersResult GetGeneralPurposeRegistersLegacy(pid_t pid,
                                                    ThreadContext* context) {
  if (ptrace(PTRACE_GETREGS, pid, nullptr, &context->t32) != 0) {
    PLOG(ERROR) << "ptrace";
    return GetRegistersResult::kError;
  }
  return GetRegistersResult::k32Bit;
}

GetRegistersResult GetFloatingPointRegistersLegacy(pid_t pid,
                                                   FloatContext* context) {
  if (ptrace(PTRACE_GETFPREGS, pid, nullptr, &context->f32.fpregs) != 0) {
    PLOG(ERROR) << "ptrace";
    return GetRegistersResult::kError;
  }
  context->f32.have_fpregs = true;

  if (ptrace(PTRACE_GETVFPREGS, pid, nullptr, &context->f32.vfp) != 0) {
    switch (errno) {
      case EINVAL:
        // These registers are optional on 32-bit ARM cpus
        break;
      default:
        PLOG(ERROR) << "ptrace";
        return GetRegistersResult::kError;
    }
  } else {
    context->f32.have_vfp = true;
  }
  return GetRegistersResult::k32Bit;
}
#endif  // ARCH_CPU_ARMEL

#if defined(ARCH_CPU_ARM_FAMILY)
// Normally, the Linux kernel will copy out register sets according to the size
// of the struct that contains them. Tracing a 32-bit ARM process running in
// compatibility mode on a 64-bit ARM cpu will only copy data for the number of
// registers times the size of the register, which won't include any possible
// trailing padding in the struct. These are the sizes of the register data, not
// including any possible padding.
constexpr size_t kArmVfpSize = 32 * 8 + 4;
constexpr size_t kArmFpregsSize = 11 * 4 + 8;
#endif  // ARCH_CPU_ARM_FAMILY

}  // namespace

ThreadContext::ThreadContext() {
  memset(this, 0, sizeof(*this));
}

ThreadContext::~ThreadContext() {}

FloatContext::FloatContext() {
  memset(this, 0, sizeof(*this));
}

FloatContext::~FloatContext() {}

GetRegistersResult GetGeneralPurposeRegisters(pid_t pid,
                                              ThreadContext* context) {
  iovec iov;
  iov.iov_base = context;
  iov.iov_len = sizeof(*context);
  if (ptrace(
          PTRACE_GETREGSET, pid, reinterpret_cast<void*>(NT_PRSTATUS), &iov) !=
      0) {
    switch (errno) {
#if defined(ARCH_CPU_ARMEL)
      case EIO:
        return GetGeneralPurposeRegistersLegacy(pid, context);
#endif  // ARCH_CPU_ARMEL
      default:
        PLOG(ERROR) << "ptrace";
        return GetRegistersResult::kError;
    }
  }
  if (iov.iov_len == sizeof(context->t64)) {
    return GetRegistersResult::k64Bit;
  }
  if (iov.iov_len == sizeof(context->t32)) {
    return GetRegistersResult::k32Bit;
  }
  LOG(ERROR) << "Unexpected registers size";
  return GetRegistersResult::kError;
}

GetRegistersResult GetFloatingPointRegisters(pid_t pid, FloatContext* context) {
#if defined(ARCH_CPU_X86_FAMILY)
  iovec iov;
  iov.iov_base = context;
  iov.iov_len = sizeof(*context);
  if (ptrace(
          PTRACE_GETREGSET, pid, reinterpret_cast<void*>(NT_PRFPREG), &iov) !=
      0) {
    PLOG(ERROR) << "ptrace";
    return GetRegistersResult::kError;
  }
  if (iov.iov_len == sizeof(context->f64)) {
    return GetRegistersResult::k64Bit;
  }
  if (iov.iov_len == sizeof(context->f32)) {
    return GetRegistersResult::k32Bit;
  }
  LOG(ERROR) << "Unexpected registers size";
  return GetRegistersResult::kError;

#elif defined(ARCH_CPU_ARM_FAMILY)
  // 64-bit processes should only have registers collected with NT_PRFPREG
  // 32-bit processes may have one or two register sets collected with
  // NT_PRFPREG and/or NT_ARM_VFP, depending on configuration.
  context->f32.have_fpregs = false;
  context->f32.have_vfp = false;

  iovec iov;
  iov.iov_base = context;
  iov.iov_len = sizeof(*context);
  if (ptrace(
          PTRACE_GETREGSET, pid, reinterpret_cast<void*>(NT_PRFPREG), &iov) !=
      0) {
    switch (errno) {
#if defined(ARCH_CPU_ARMEL)
      case EIO:
        return GetFloatingPointRegistersLegacy(pid, context);
#endif  // ARCH_CPU_ARMEL
      case EINVAL:
        // A 32-bit process running on a 64-bit CPU doesn't have this register
        // set. It should have a VFP register set instead.
        break;
      default:
        PLOG(ERROR) << "ptrace";
        return GetRegistersResult::kError;
    }
  } else {
    if (iov.iov_len == sizeof(context->f64)) {
      // Target process is 64-bit; there are no other registers sets.
      return GetRegistersResult::k64Bit;
    }
    if (iov.iov_len == kArmFpregsSize ||
        iov.iov_len == sizeof(context->f32.fpregs)) {
      context->f32.have_fpregs = true;
    } else {
      LOG(ERROR) << "Unexpected registers size";
      return GetRegistersResult::kError;
    }
  }

  iov.iov_base = &context->f32.vfp;
  iov.iov_len = sizeof(context->f32.vfp);
  if (ptrace(
          PTRACE_GETREGSET, pid, reinterpret_cast<void*>(NT_ARM_VFP), &iov) !=
      0) {
    switch (errno) {
      case EINVAL:
        // VFP may not be present on 32-bit ARM cpus.
        break;
      default:
        PLOG(ERROR) << "ptrace";
        return GetRegistersResult::kError;
    }
  } else {
    if (iov.iov_len == kArmVfpSize) {
      context->f32.have_vfp = true;
    } else {
      LOG(ERROR) << "Unexpected registers size";
      return GetRegistersResult::kError;
    }
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

bool GetThreadArea(pid_t pid, LinuxVMAddress* address) {
#if defined(ARCH_CPU_ARM_FAMILY)
  // GETREGSET should work for both 32 and 64 bit processes on a 64-bit kernel.
  iovec iov;
  iov.iov_base = address;
  iov.iov_len = sizeof(*address);
  if (ptrace(
          PTRACE_GETREGSET, pid, reinterpret_cast<void*>(NT_ARM_TLS), &iov) !=
      0) {
    switch (errno) {
#if defined(ARCH_CPU_ARMEL)
      case EIO:
      case EINVAL:
        // 32-bit kernels don't support NT_ARM_TLS and may not support GETREGSET
        // so we need to use GET_THREAD_AREA instead.
        break;
#endif  // ARCH_CPU_ARMEL
      default:
        PLOG(ERROR) << "ptrace";
        return false;
    }
  } else {
    if (iov.iov_len != sizeof(*address)) {
      LOG(ERROR) << "thread address size mismatch";
      return false;
    }
    return true;
  }

#if defined(ARCH_CPU_ARMEL)
  void* result;
  if (ptrace(
          PTRACE_GET_THREAD_AREA, pid, reinterpret_cast<void*>(0), &result) !=
      0) {
    PLOG(ERROR) << "ptrace";
    return false;
  }
  *address = FromPointerCast<LinuxVMAddress>(result);
  return true;
#endif  // ARCH_CPU_ARMEL

#elif defined(ARCH_CPU_X86_FAMILY)
  user_desc desc;
  iovec iov;
  iov.iov_base = &desc;
  iov.iov_len = sizeof(desc);
  *address = 0;
  if (ptrace(
          PTRACE_GETREGSET, pid, reinterpret_cast<void*>(NT_386_TLS), &iov) !=
      0) {
    switch (errno) {
      case EINVAL:
        // NT_386_TLS is only valid if the target is 32-bit.
        break;
      default:
        PLOG(ERROR) << "ptrace";
        return false;
    }
  } else {
    *address = desc.base_addr;
    return true;
  }

  if (ptrace(PTRACE_ARCH_PRCTL,
             pid,
             reinterpret_cast<void*>(address),
             reinterpret_cast<void*>(ARCH_GET_FS)) != 0) {
    PLOG(ERROR) << "ptrace";
    return false;
  }
  return true;
#endif  // ARCH_CPU_ARM_FAMILY
}

}  // namespace crashpad
