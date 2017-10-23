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

#include "util/linux/ptracer.h"

#include <linux/elf.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/uio.h>

#include "base/logging.h"
#include "build/build_config.h"
#include "util/misc/from_pointer_cast.h"

#if defined(ARCH_CPU_X86_FAMILY)
#include <asm/ldt.h>
#endif

namespace crashpad {

namespace {

#if defined(ARCH_CPU_X86_FAMILY)

template <typename Destination>
bool GetRegisterSet(pid_t tid, int set, Destination* dest) {
  iovec iov;
  iov.iov_base = dest;
  iov.iov_len = sizeof(*dest);
  if (ptrace(PTRACE_GETREGSET, tid, reinterpret_cast<void*>(set), &iov) != 0) {
    PLOG(ERROR) << "ptrace";
    return false;
  }
  if (iov.iov_len != sizeof(*dest)) {
    LOG(ERROR) << "Unexpected registers size";
    return false;
  }
  return true;
}

bool GetFloatingPointRegisters32(pid_t tid, FloatContext* context) {
  return GetRegisterSet(tid, NT_PRXFPREG, &context->f32.fxsave);
}

bool GetFloatingPointRegisters64(pid_t tid, FloatContext* context) {
  return GetRegisterSet(tid, NT_PRFPREG, &context->f64.fxsave);
}

bool GetThreadArea32(pid_t tid,
                     const ThreadContext& context,
                     LinuxVMAddress* address) {
  size_t index = (context.t32.xgs & 0xffff) >> 3;
  user_desc desc;
  if (ptrace(
          PTRACE_GET_THREAD_AREA, tid, reinterpret_cast<void*>(index), &desc) !=
      0) {
    PLOG(ERROR) << "ptrace";
    return false;
  }

  *address = desc.base_addr;
  return true;
}

bool GetThreadArea64(pid_t tid,
                     const ThreadContext& context,
                     LinuxVMAddress* address) {
  *address = context.t64.fs_base;
  return true;
}

#elif defined(ARCH_CPU_ARM_FAMILY)

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

bool GetGeneralPurposeRegistersLegacy(pid_t tid, ThreadContext* context) {
  if (ptrace(PTRACE_GETREGS, tid, nullptr, &context->t32) != 0) {
    PLOG(ERROR) << "ptrace";
    return false;
  }
  return true;
}

bool GetFloatingPointRegistersLegacy(pid_t tid, FloatContext* context) {
  if (ptrace(PTRACE_GETFPREGS, tid, nullptr, &context->f32.fpregs) != 0) {
    PLOG(ERROR) << "ptrace";
    return false;
  }
  context->f32.have_fpregs = true;

  if (ptrace(PTRACE_GETVFPREGS, tid, nullptr, &context->f32.vfp) != 0) {
    switch (errno) {
      case EINVAL:
        // These registers are optional on 32-bit ARM cpus
        break;
      default:
        PLOG(ERROR) << "ptrace";
        return false;
    }
  } else {
    context->f32.have_vfp = true;
  }
  return true;
}
#endif  // ARCH_CPU_ARMEL

// Normally, the Linux kernel will copy out register sets according to the size
// of the struct that contains them. Tracing a 32-bit ARM process running in
// compatibility mode on a 64-bit ARM cpu will only copy data for the number of
// registers times the size of the register, which won't include any possible
// trailing padding in the struct. These are the sizes of the register data, not
// including any possible padding.
constexpr size_t kArmVfpSize = 32 * 8 + 4;

// Target is 32-bit
bool GetFloatingPointRegisters32(pid_t tid, FloatContext* context) {
  context->f32.have_fpregs = false;
  context->f32.have_vfp = false;

  iovec iov;
  iov.iov_base = &context->f32.fpregs;
  iov.iov_len = sizeof(context->f32.fpregs);
  if (ptrace(
          PTRACE_GETREGSET, tid, reinterpret_cast<void*>(NT_PRFPREG), &iov) !=
      0) {
    switch (errno) {
#if defined(ARCH_CPU_ARMEL)
      case EIO:
        return GetFloatingPointRegistersLegacy(tid, context);
#endif  // ARCH_CPU_ARMEL
      case EINVAL:
        // A 32-bit process running on a 64-bit CPU doesn't have this register
        // set. It should have a VFP register set instead.
        break;
      default:
        PLOG(ERROR) << "ptrace";
        return false;
    }
  } else {
    if (iov.iov_len != sizeof(context->f32.fpregs)) {
      LOG(ERROR) << "Unexpected registers size";
      return false;
    }
    context->f32.have_fpregs = true;
  }

  iov.iov_base = &context->f32.vfp;
  iov.iov_len = sizeof(context->f32.vfp);
  if (ptrace(
          PTRACE_GETREGSET, tid, reinterpret_cast<void*>(NT_ARM_VFP), &iov) !=
      0) {
    switch (errno) {
      case EINVAL:
        // VFP may not be present on 32-bit ARM cpus.
        break;
      default:
        PLOG(ERROR) << "ptrace";
        return false;
    }
  } else {
    if (iov.iov_len != kArmVfpSize && iov.iov_len != sizeof(context->f32.vfp)) {
      LOG(ERROR) << "Unexpected registers size";
      return false;
    }
    context->f32.have_vfp = true;
  }

  if (!(context->f32.have_fpregs || context->f32.have_vfp)) {
    LOG(ERROR) << "Unable to collect registers";
    return false;
  }
  return true;
}

bool GetFloatingPointRegisters64(pid_t tid, FloatContext* context) {
  iovec iov;
  iov.iov_base = context;
  iov.iov_len = sizeof(*context);
  if (ptrace(
          PTRACE_GETREGSET, tid, reinterpret_cast<void*>(NT_PRFPREG), &iov) !=
      0) {
    PLOG(ERROR) << "ptrace";
    return false;
  }
  if (iov.iov_len != sizeof(context->f64)) {
    LOG(ERROR) << "Unexpected registers size";
    return false;
  }
  return true;
}

bool GetThreadArea32(pid_t tid,
                     const ThreadContext& context,
                     LinuxVMAddress* address) {
#if defined(ARCH_CPU_ARMEL)
  void* result;
  if (ptrace(PTRACE_GET_THREAD_AREA, tid, nullptr, &result) != 0) {
    PLOG(ERROR) << "ptrace";
    return false;
  }
  *address = FromPointerCast<LinuxVMAddress>(result);
  return true;
#else
  // TODO(jperaza): it doesn't look like there is a way for a 64-bit ARM process
  // to get the thread area for a 32-bit ARM process with ptrace.
  LOG(WARNING) << "64-bit ARM cannot trace TLS area for a 32-bit process";
  return false;
#endif  // ARCH_CPU_ARMEL
}

bool GetThreadArea64(pid_t tid,
                     const ThreadContext& context,
                     LinuxVMAddress* address) {
  iovec iov;
  iov.iov_base = address;
  iov.iov_len = sizeof(*address);
  if (ptrace(
          PTRACE_GETREGSET, tid, reinterpret_cast<void*>(NT_ARM_TLS), &iov) !=
      0) {
    PLOG(ERROR) << "ptrace";
    return false;
  }
  if (iov.iov_len != 8) {
    LOG(ERROR) << "address size mismatch";
    return false;
  }
  return true;
}
#else
#error Port.
#endif  // ARCH_CPU_X86_FAMILY

size_t GetGeneralPurposeRegistersAndLength(pid_t tid, ThreadContext* context) {
  iovec iov;
  iov.iov_base = context;
  iov.iov_len = sizeof(*context);
  if (ptrace(
          PTRACE_GETREGSET, tid, reinterpret_cast<void*>(NT_PRSTATUS), &iov) !=
      0) {
    switch (errno) {
#if defined(ARCH_CPU_ARMEL)
      case EIO:
        if (GetGeneralPurposeRegistersLegacy(tid, context)) {
          return sizeof(context->t32);
        }
#endif  // ARCH_CPU_ARMEL
      default:
        PLOG(ERROR) << "ptrace";
        return 0;
    }
  }
  return iov.iov_len;
}

bool GetGeneralPurposeRegisters32(pid_t tid, ThreadContext* context) {
  if (GetGeneralPurposeRegistersAndLength(tid, context) !=
      sizeof(context->t32)) {
    LOG(ERROR) << "Unexpected registers size";
    return false;
  }
  return true;
}

bool GetGeneralPurposeRegisters64(pid_t tid, ThreadContext* context) {
  if (GetGeneralPurposeRegistersAndLength(tid, context) !=
      sizeof(context->t64)) {
    LOG(ERROR) << "Unexpected registers size";
    return false;
  }
  return true;
}

}  // namespace

Ptracer::Ptracer() : is_64_bit_(false), initialized_() {}

Ptracer::Ptracer(bool is_64_bit) : is_64_bit_(is_64_bit) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);
  INITIALIZATION_STATE_SET_VALID(initialized_);
}

Ptracer::~Ptracer() {}

bool Ptracer::Initialize(pid_t pid) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  ThreadContext context;
  size_t length = GetGeneralPurposeRegistersAndLength(pid, &context);
  if (length == sizeof(context.t64)) {
    is_64_bit_ = true;
  } else if (length == sizeof(context.t32)) {
    is_64_bit_ = false;
  } else {
    LOG(ERROR) << "Unexpected registers size";
    return false;
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

bool Ptracer::Is64Bit() {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return is_64_bit_;
}

bool Ptracer::GetThreadInfo(pid_t tid, ThreadInfo* info) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  if (is_64_bit_) {
    return GetGeneralPurposeRegisters64(tid, &info->thread_context) &&
           GetFloatingPointRegisters64(tid, &info->float_context) &&
           GetThreadArea64(
               tid, info->thread_context, &info->thread_specific_data_address);
  }

  return GetGeneralPurposeRegisters32(tid, &info->thread_context) &&
         GetFloatingPointRegisters32(tid, &info->float_context) &&
         GetThreadArea32(
             tid, info->thread_context, &info->thread_specific_data_address);
}

}  // namespace crashpad
