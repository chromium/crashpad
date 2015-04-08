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

#ifndef CRASHPAD_UTIL_MACH_EXCEPTION_TYPES_H_
#define CRASHPAD_UTIL_MACH_EXCEPTION_TYPES_H_

#include <mach/mach.h>
#include <sys/types.h>

namespace crashpad {

//! \brief Recovers the original exception, first exception code, and signal
//!     from the encoded form of the first exception code delivered with
//!     `EXC_CRASH` exceptions.
//!
//! `EXC_CRASH` exceptions are generated when the kernel has committed to
//! terminating a process as a result of a core-generating POSIX signal and, for
//! hardware exceptions, an earlier Mach exception. Information about this
//! earlier exception and signal is made available to the `EXC_CRASH` handler
//! via its `code[0]` parameter. This function recovers the original exception,
//! the value of `code[0]` from the original exception, and the value of the
//! signal responsible for process termination.
//!
//! \param[in] code_0 The first exception code (`code[0]`) passed to a Mach
//!     exception handler in an `EXC_CRASH` exception. It is invalid to call
//!     this function with an exception code from any exception other than
//!     `EXC_CRASH`.
//! \param[out] original_code_0 The first exception code (`code[0]`) passed to
//!     the Mach exception handler for a hardware exception that resulted in the
//!     generation of a POSIX signal that caused process termination. If the
//!     signal that caused termination was not sent as a result of a hardware
//!     exception, this will be `0`. Callers that do not need this value may
//!     pass `nullptr`.
//! \param[out] signal The POSIX signal that caused process termination. Callers
//!     that do not need this value may pass `nullptr`.
//!
//! \return The original exception for a hardware exception that resulted in the
//!     generation of a POSIX signal that caused process termination. If the
//!     signal that caused termination was not sent as a result of a hardware
//!     exception, this will be `0`.
exception_type_t ExcCrashRecoverOriginalException(
    mach_exception_code_t code_0,
    mach_exception_code_t* original_code_0,
    int* signal);

//! \brief Determines whether an exception is a non-fatal `EXC_RESOURCE`.
//!
//! \param[in] exception The exception type as received by a Mach exception
//!     handler.
//! \param[in] code_0 The first exception code (`code[0]`) as received by a
//!     Mach exception handler.
//! \param[in] pid The process ID that the exception occurred in. In some cases,
//!     process may need to be queried to determine whether an `EXC_RESOURCE`
//!     exception is fatal.
//!
//! \return `true` if the exception is a non-fatal `EXC_RESOURCE`. `false`
//!     otherwise. If the exception is `EXC_RESOURCE` of a recognized type but
//!     it is not possible to determine whether it is fatal, returns `true`
//!     under the assumption that all known `EXC_RESOURCE` exceptions are
//!     non-fatal by default. If the exception is not `EXC_RESOURCE` or is an
//!     unknown `EXC_RESOURCE` type, returns `false`.
bool IsExceptionNonfatalResource(exception_type_t exception,
                                 mach_exception_code_t code_0,
                                 pid_t pid);

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_MACH_EXCEPTION_TYPES_H_
