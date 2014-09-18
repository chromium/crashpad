// Copyright 2014 The Crashpad Authors. All rights reserved.
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

#include "util/mach/mach_extensions.h"

#include <AvailabilityMacros.h>
#include <pthread.h>

#include "util/mac/mac_util.h"

namespace crashpad {

thread_t MachThreadSelf() {
  // The pthreads library keeps its own copy of the thread port. Using it does
  // not increment its reference count.
  return pthread_mach_thread_np(pthread_self());
}

exception_mask_t ExcMaskAll() {
  // This is necessary because of the way that the kernel validates
  // exception_mask_t arguments to
  // {host,task,thread}_{get,set,swap}_exception_ports(). It is strict,
  // rejecting attempts to operate on any bits that it does not recognize. See
  // 10.9.4 xnu-2422.110.17/osfmk/mach/ipc_host.c and
  // xnu-2422.110.17/osfmk/mach/ipc_tt.c.

#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_9
  int mac_os_x_minor_version = MacOSXMinorVersion();
#endif

  // See 10.6.8 xnu-1504.15.3/osfmk/mach/exception_types.h. 10.7 uses the same
  // definition as 10.6. See 10.7.5 xnu-1699.32.7/osfmk/mach/exception_types.h
  const exception_mask_t kExcMaskAll_10_6 =
      EXC_MASK_BAD_ACCESS | EXC_MASK_BAD_INSTRUCTION | EXC_MASK_ARITHMETIC |
      EXC_MASK_EMULATION | EXC_MASK_SOFTWARE | EXC_MASK_BREAKPOINT |
      EXC_MASK_SYSCALL | EXC_MASK_MACH_SYSCALL | EXC_MASK_RPC_ALERT |
      EXC_MASK_MACHINE;
#if MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_7
  if (mac_os_x_minor_version <= 7) {
    return kExcMaskAll_10_6;
  }
#endif

  // 10.8 added EXC_MASK_RESOURCE. See 10.8.5
  // xnu-2050.48.11/osfmk/mach/exception_types.h.
  const exception_mask_t kExcMaskAll_10_8 =
      kExcMaskAll_10_6 | EXC_MASK_RESOURCE;
#if MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_8
  if (mac_os_x_minor_version <= 8) {
    return kExcMaskAll_10_8;
  }
#endif

  // 10.9 added EXC_MASK_GUARD. See 10.9.4
  // xnu-2422.110.17/osfmk/mach/exception_types.h.
  const exception_mask_t kExcMaskAll_10_9 = kExcMaskAll_10_8 | EXC_MASK_GUARD;
  return kExcMaskAll_10_9;
}

}  // namespace crashpad
