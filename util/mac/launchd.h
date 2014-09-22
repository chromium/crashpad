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

#ifndef CRASHPAD_UTIL_MAC_LAUNCHD_H_
#define CRASHPAD_UTIL_MAC_LAUNCHD_H_

#include <CoreFoundation/CoreFoundation.h>
#include <launch.h>

namespace crashpad {

//! \brief Converts a Core Foundation-type property list to a launchd-type
//!     `launch_data_t`.
//!
//! \param[in] property_cf The Core Foundation-type property list to convert.
//!
//! \return The converted launchd-type `launch_data_t`. The caller takes
//!     ownership of the returned value. On error, returns `NULL`.
//!
//! \note This function handles all `CFPropertyListRef` types except for
//!     `CFDateRef`, because there’s no `launch_data_type_t` analogue. Not all
//!     types supported in a launchd-type `launch_data_t` have
//!     `CFPropertyListRef` analogues.
launch_data_t CFPropertyToLaunchData(CFPropertyListRef property_cf);

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_MAC_LAUNCHD_H_
