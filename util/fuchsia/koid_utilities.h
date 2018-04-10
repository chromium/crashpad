// Copyright 2018 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_UTIL_FUCHSIA_KOID_UTILITIES_H_
#define CRASHPAD_UTIL_FUCHSIA_KOID_UTILITIES_H_

#include <zircon/types.h>

#include "base/fuchsia/scoped_zx_handle.h"

namespace crashpad {

//! \brief Gets a process handle given the process' koid.
//!
//! \param[in] koid The process id.
//! \return A zx_handle_t (owned by a base::ScopedZxHandle) for the process. If
//!     the handle is invalid, an error will have been logged.
base::ScopedZxHandle GetProcessFromKoid(zx_koid_t koid);

//! \brief Retrieves the koid for a given object handle.
//!
//! \param[in] object The handle for which the koid is to be retrieved.
//! \return The koid of \a handle, or `ZX_HANDLE_INVALID` with an error logged.
zx_koid_t GetKoidForHandle(zx_handle_t object);

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_FUCHSIA_KOID_UTILITIES_H_
