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

#ifndef CRASHPAD_UTIL_MAC_SERVICE_MANAGEMENT_H_
#define CRASHPAD_UTIL_MAC_SERVICE_MANAGEMENT_H_

#include <CoreFoundation/CoreFoundation.h>
#include <unistd.h>

#include <string>

namespace crashpad {

//! \brief Submits a job to the user launchd domain as in `SMJobSubmit()`.
//!
//! \param[in] job_cf A dictionary describing a job.
//!
//! \return `true` if the job was submitted successfully, otherwise `false`.
//!
//! \note This function is provided because `SMJobSubmit()` does not exist prior
//!     to Mac OS X 10.6, and behaves flakily on Mac OS X 10.10 DP5 14A314h in
//!     that it hangs 25% of the time (radar 17365104).
bool ServiceManagementSubmitJob(CFDictionaryRef job_cf);

//! \brief Removes a job from the user launchd domain as in `SMJobRemove()`.
//!
//! \param[in] label The label for the job to remove.
//! \param[in] wait `true` if this function should block, waiting for the job to
//!     be removed. `false` if the job may be removed asynchronously.
//!
//! \return `true` if the job was removed successfully or if an asynchronous
//!     attempt to remove the job was started successfully, otherwise `false`.
//!
//! \note This function is provided because `SMJobRemove()` does not exist prior
//!     to Mac OS X 10.6, and when \a wait is true, blocks for far too long on
//!     Mac OS X 10.10 DP5 14A314h (radar 17365104; `_block_until_job_exits()`
//!     contains a one-second sleep() call).
bool ServiceManagementRemoveJob(const std::string& label, bool wait);

//! \brief Determines whether a specified job is loaded in the user launchd
//!     domain.
//!
//! \param[in] label The label for the job to look up.
//!
//! \return `true` if the job is loaded, otherwise `false`.
//!
//! \note A loaded job is not necessarily presently running, nor has it
//!     necessarily ever run in the past.
//! \note This function is provided because `SMJobCopyDictionary()` does not
//!     exist prior to Mac OS X 10.6, and on Mac OS X 10.10 DP5 14A314h, it
//!     fails to return a job dictionary immediately after a job is loaded
//!     (radar 17365104).
bool ServiceManagementIsJobLoaded(const std::string& label);

//! \brief Determines whether a specified job is running in the user launchd
//!     domain.
//!
//! \param[in] label The label for the job to look up.
//!
//! \return The job’s process ID if running, otherwise `0`.
//!
//! \note This function is provided because `SMJobCopyDictionary()` does not
//!     exist prior to Mac OS X 10.6, and on Mac OS X 10.10 DP5 14A314h, it
//!     fails to return a job dictionary immediately after a job is loaded
//!     (radar 17365104).
pid_t ServiceManagementIsJobRunning(const std::string& label);

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_MAC_SERVICE_MANAGEMENT
