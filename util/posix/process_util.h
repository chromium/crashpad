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

#ifndef CRASHPAD_UTIL_POSIX_PROCESS_UTIL_POSIX_H_
#define CRASHPAD_UTIL_POSIX_PROCESS_UTIL_POSIX_H_

#include <unistd.h>

#include <string>
#include <vector>

namespace crashpad {

//! \brief Obtains the arguments used to launch a process.
//!
//! \param[in] pid The process ID of the process to examine.
//! \param[out] argv The process’ arguments as passed to its `main()` function
//!     as the \a argv parameter, possibly modified by the process.
//!
//! \return `true` on success, with \a argv populated appropriately. Otherwise,
//!     `false`.
//!
//! \note This function may spuriously return `false` when used to examine a
//!     process that it is calling `exec()`. If examining such a process, call
//!     this function in a retry loop with a small (100ns) delay to avoid an
//!     erroneous assumption that \a pid is not running.
bool ProcessArgumentsForPID(pid_t pid, std::vector<std::string>* argv);

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_POSIX_PROCESS_UTIL_POSIX_H_
