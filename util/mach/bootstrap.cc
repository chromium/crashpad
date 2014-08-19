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

#include "util/mach/bootstrap.h"

#include <AvailabilityMacros.h>
#include <servers/bootstrap.h>

#include "base/basictypes.h"
#include "base/mac/scoped_mach_port.h"
#include "util/mac/mac_util.h"

#if MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_5
namespace {

// Wraps bootstrap_register to avoid the deprecation warning. It needs to be
// used on 10.5.
kern_return_t BootstrapRegister(mach_port_t bp,
                                name_t service_name,
                                mach_port_t sp) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  return bootstrap_register(bp, service_name, sp);
#pragma GCC diagnostic pop
}

}  // namespace
#endif

namespace crashpad {

kern_return_t BootstrapCheckIn(mach_port_t bp,
                               const std::string& service_name,
                               mach_port_t* service_port) {
  // bootstrap_check_in (until the 10.6 SDK) and bootstrap_register (all SDKs)
  // are declared with a char* argument, but they don’t actually modify the char
  // data, so this is safe.
  char* service_name_mutable = const_cast<char*>(service_name.c_str());

  kern_return_t kr = bootstrap_check_in(bp, service_name_mutable, service_port);

#if MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_5
  if (kr == BOOTSTRAP_UNKNOWN_SERVICE && MacOSXMinorVersion() <= 5) {
    // This code path should only be entered on 10.5 or earlier.
    mach_port_t local_service_port;
    kr = mach_port_allocate(
        mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &local_service_port);
    if (kr != KERN_SUCCESS) {
      return kr;
    }
    base::mac::ScopedMachReceiveRight service_port_receive_right_owner(
        local_service_port);

    kr = mach_port_insert_right(mach_task_self(),
                                local_service_port,
                                local_service_port,
                                MACH_MSG_TYPE_MAKE_SEND);
    if (kr != KERN_SUCCESS) {
      return kr;
    }
    base::mac::ScopedMachSendRight service_port_send_right_owner(
        local_service_port);

    kr = BootstrapRegister(bp, service_name_mutable, local_service_port);
    if (kr != BOOTSTRAP_SUCCESS) {
      return kr;
    }

    ignore_result(service_port_receive_right_owner.release());
    *service_port = local_service_port;
  }
#endif

  return kr;
}

}  // namespace crashpad
