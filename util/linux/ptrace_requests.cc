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

GetRegistersResult GetGeneralPurposeRegisters(pid_t pid,
                                              ThreadContext* context) {
  iovec iov;
  iov.iov_base = context;
  iov.iov_len = sizeof(*context);
  if (ptrace(
          PTRACE_GETREGSET, pid, reinterpret_cast<void*>(NT_PRSTATUS), &iov) !=
      0) {
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

GetRegistersResult GetFloatingPointRegisters(pid_t pid, FloatContext* context) {
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
  } else if (iov.iov_len == sizeof(context->f32)) {
    return GetRegistersResult::k32Bit;
  } else {
    return GetRegistersResult::kError;
  }
}

}  // namespace crashpad
