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

#ifndef CRASHPAD_UTIL_MACH_BOOTSTRAP_H_
#define CRASHPAD_UTIL_MACH_BOOTSTRAP_H_

#include <mach/mach.h>

#include <string>

namespace crashpad {

//! \brief Calls `bootstrap_check_in()` to check in with the bootstrap server.
//!
//! \param[in] bp The bootstrap server to check in with.
//! \param[in] service_name The name of the service to check in.
//! \param[out] service_port The receive right for the checked-in service.
//!
//! \return `BOOTSTRAP_SUCCESS` on success, with \a service_port set
//!     appropriately. Otherwise, any error that might be returned by
//!     `bootstrap_check_in()`.
//!
//! This function is a wrapper around `bootstrap_check_in()`, checking in with
//! the bootstrap server at \a bp. It exists primarily for compatibility with
//! Mac OS X 10.5, where it is not possible to call `bootstrap_check_in()` for a
//! \a service_name that has not already been registered with the bootstrap
//! server using `bootstrap_register()`. `bootstrap_register()` was deprecated
//! in Mac OS X 10.5.
kern_return_t BootstrapCheckIn(mach_port_t bp,
                               const std::string& service_name,
                               mach_port_t* service_port);

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_MACH_MACH_MESSAGE_SERVER_H_
