// Copyright 2015 The Crashpad Authors. All rights reserved.
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

#include "util/mach/exception_types.h"

#include <Availability.h>
#include <AvailabilityMacros.h>
#include <dlfcn.h>
#include <errno.h>
#include <libproc.h>
#include <kern/exc_resource.h>

#include "base/logging.h"
#include "base/mac/mach_logging.h"
#include "util/mac/mac_util.h"

#if MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_9

extern "C" {

// proc_get_wakemon_params() is present in the Mac OS X 10.9 SDK, but no
// declaration is provided. This provides a declaration and marks it for weak
// import if the deployment target is below 10.9.
int proc_get_wakemon_params(pid_t pid, int* rate_hz, int* flags)
    __OSX_AVAILABLE_STARTING(__MAC_10_9, __IPHONE_7_0);

// Redeclare the method without the availability annotation to suppress the
// -Wpartial-availability warning.
int proc_get_wakemon_params(pid_t pid, int* rate_hz, int* flags);

}  // extern "C"

#else

namespace {

using ProcGetWakemonParamsType = int (*)(pid_t, int*, int*);

// The SDK doesn’t have proc_get_wakemon_params() to link against, even with
// weak import. This function returns a function pointer to it if it exists at
// runtime, or nullptr if it doesn’t. proc_get_wakemon_params() is looked up in
// the same module that provides proc_pidinfo().
ProcGetWakemonParamsType GetProcGetWakemonParams() {
  Dl_info dl_info;
  if (!dladdr(reinterpret_cast<void*>(proc_pidinfo), &dl_info)) {
    return nullptr;
  }

  void* dl_handle =
      dlopen(dl_info.dli_fname, RTLD_LAZY | RTLD_LOCAL | RTLD_NOLOAD);
  if (!dl_handle) {
    return nullptr;
  }

  ProcGetWakemonParamsType proc_get_wakemon_params =
      reinterpret_cast<ProcGetWakemonParamsType>(
          dlsym(dl_handle, "proc_get_wakemon_params"));
  return proc_get_wakemon_params;
}

}  // namespace

#endif

namespace {

// Wraps proc_get_wakemon_params(), calling it if the system provides it. It’s
// present on Mac OS X 10.9 and later. If it’s not available, sets errno to
// ENOSYS and returns -1.
int ProcGetWakemonParams(pid_t pid, int* rate_hz, int* flags) {
#if MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_9
  // proc_get_wakemon_params() isn’t in the SDK. Look it up dynamically.
  static ProcGetWakemonParamsType proc_get_wakemon_params =
      GetProcGetWakemonParams();
#endif

#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_9
  // proc_get_wakemon_params() is definitely available if the deployment target
  // is 10.9 or newer.
  if (!proc_get_wakemon_params) {
    errno = ENOSYS;
    return -1;
  }
#endif

  return proc_get_wakemon_params(pid, rate_hz, flags);
}

}  // namespace

namespace crashpad {

exception_type_t ExcCrashRecoverOriginalException(
    mach_exception_code_t code_0,
    mach_exception_code_t* original_code_0,
    int* signal) {
  // 10.9.4 xnu-2422.110.17/bsd/kern/kern_exit.c proc_prepareexit() sets code[0]
  // based on the signal value, original exception type, and low 20 bits of the
  // original code[0] before calling xnu-2422.110.17/osfmk/kern/exception.c
  // task_exception_notify() to raise an EXC_CRASH.
  //
  // The list of core-generating signals (as used in proc_prepareexit()’s call
  // to hassigprop()) is in 10.9.4 xnu-2422.110.17/bsd/sys/signalvar.h sigprop:
  // entires with SA_CORE are in the set. These signals are SIGQUIT, SIGILL,
  // SIGTRAP, SIGABRT, SIGEMT, SIGFPE, SIGBUS, SIGSEGV, and SIGSYS. Processes
  // killed for code-signing reasons will be killed by SIGKILL and are also
  // eligible for EXC_CRASH handling, but processes killed by SIGKILL for other
  // reasons are not.
  if (signal) {
    *signal = (code_0 >> 24) & 0xff;
  }

  if (original_code_0) {
    *original_code_0 = code_0 & 0xfffff;
  }

  return (code_0 >> 20) & 0xf;
}

bool IsExceptionNonfatalResource(exception_type_t exception,
                                 mach_exception_code_t code_0,
                                 pid_t pid) {
  if (exception != EXC_RESOURCE) {
    return false;
  }

  const int resource_type = EXC_RESOURCE_DECODE_RESOURCE_TYPE(code_0);
  const int resource_flavor = EXC_RESOURCE_DECODE_FLAVOR(code_0);

  if (resource_type == RESOURCE_TYPE_CPU &&
      (resource_flavor == FLAVOR_CPU_MONITOR ||
       resource_flavor == FLAVOR_CPU_MONITOR_FATAL)) {
    // These exceptions may be fatal. They are not fatal by default at task
    // creation but can be made fatal by calling proc_rlimit_control() with
    // RLIMIT_CPU_USAGE_MONITOR as the second argument and CPUMON_MAKE_FATAL set
    // in the flags.
    if (MacOSXMinorVersion() >= 10) {
      // In Mac OS X 10.10, the exception code indicates whether the exception
      // is fatal. See 10.10 xnu-2782.1.97/osfmk/kern/thread.c
      // THIS_THREAD_IS_CONSUMING_TOO_MUCH_CPU__SENDING_EXC_RESOURCE().
      return resource_flavor == FLAVOR_CPU_MONITOR;
    }

    // In Mac OS X 10.9, there’s no way to determine whether the exception is
    // fatal. Unlike RESOURCE_TYPE_WAKEUPS below, there’s no way to determine
    // this outside the kernel. proc_rlimit_control()’s RLIMIT_CPU_USAGE_MONITOR
    // is the only interface to modify CPUMON_MAKE_FATAL, but it’s only able to
    // set this bit, not obtain its current value.
    //
    // Default to assuming that these exceptions are nonfatal. They are nonfatal
    // by default and no users of proc_rlimit_control() were found on 10.9.5
    // 13F1066 in /System and /usr outside of Metadata.framework and associated
    // tools.
    return true;
  }

  if (resource_type == RESOURCE_TYPE_WAKEUPS &&
      resource_flavor == FLAVOR_WAKEUPS_MONITOR) {
    // These exceptions may be fatal. They are not fatal by default at task
    // creation, but can be made fatal by calling proc_rlimit_control() with
    // RLIMIT_WAKEUPS_MONITOR as the second argument and WAKEMON_MAKE_FATAL set
    // in the flags.
    //
    // proc_get_wakemon_params() (which calls
    // through to proc_rlimit_control() with RLIMIT_WAKEUPS_MONITOR) determines
    // whether these exceptions are fatal. See 10.10
    // xnu-2782.1.97/osfmk/kern/task.c
    // THIS_PROCESS_IS_CAUSING_TOO_MANY_WAKEUPS__SENDING_EXC_RESOURCE().
    //
    // If proc_get_wakemon_params() fails, default to assuming that these
    // exceptions are nonfatal. They are nonfatal by default and no users of
    // proc_rlimit_control() were found on 10.9.5 13F1066 in /System and /usr
    // outside of Metadata.framework and associated tools.
    int wm_rate;
    int wm_flags;
    int rv = ProcGetWakemonParams(pid, &wm_rate, &wm_flags);
    if (rv < 0) {
      PLOG(WARNING) << "ProcGetWakemonParams";
      return true;
    }

    return !(wm_flags & WAKEMON_MAKE_FATAL);
  }

  if (resource_type == RESOURCE_TYPE_MEMORY &&
      resource_flavor == FLAVOR_HIGH_WATERMARK) {
    // These exceptions are never fatal. See 10.10
    // xnu-2782.1.97/osfmk/kern/task.c
    // THIS_PROCESS_CROSSED_HIGH_WATERMARK__SENDING_EXC_RESOURCE().
    return true;
  }

  // Treat unknown exceptions as fatal. This is the conservative approach: it
  // may result in more crash reports being generated, but the type-flavor
  // combinations can be evaluated to determine appropriate handling.
  LOG(WARNING) << "unknown resource type " << resource_type << " flavor "
               << resource_flavor;
  return false;
}

}  // namespace crashpad
