// Copyright 2019 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_UTIL_PROCESS_PROCESS_ID_H_
#define CRASHPAD_UTIL_PROCESS_PROCESS_ID_H_

#include "base/format_macros.h"
#include "build/build_config.h"

#if defined(OS_FUCHSIA)
#include <zircon/types.h>
#elif defined(OS_POSIX)
#include <sys/types.h>
#elif defined(OS_WIN)
#include <windows.h>
#endif

namespace crashpad {

#if defined(OS_FUCHSIA)
using ProcessID = zx_koid_t;
#define PRI_PROCESS_ID PRId64
#define PROCESS_ID_PRINT_CAST(X) (X)
#elif defined(OS_POSIX) || DOXYGEN
//! \brief Alias for platform-specific type to represent a process.
using ProcessID = pid_t;
#define PRI_PROCESS_ID PRIdMAX
#define PROCESS_ID_PRINT_CAST(X) static_cast<intmax_t>(X)
#elif defined(OS_WIN)
using ProcessID = DWORD;
#define PRI_PROCESS_ID "lu"
#define PROCESS_ID_PRINT_CAST(X) (X)
#else
#error Port.
#endif

extern const ProcessID kInvalidProcessID;

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_PROCESS_PROCESS_ID_H_
