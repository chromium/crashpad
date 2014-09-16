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

#ifndef CRASHPAD_UTIL_MACH_MACH_EXTENSIONS_H_
#define CRASHPAD_UTIL_MACH_MACH_EXTENSIONS_H_

#include <mach/mach.h>

namespace crashpad {

//! \brief `MACH_PORT_NULL` with the correct type for a Mach port,
//!     `mach_port_t`.
//!
//! For situations where implicit conversions between signed and unsigned types
//! are not performed, use kMachPortNull instead of an explicit `static_cast` of
//! `MACH_PORT_NULL` to `mach_port_t`. This is useful for logging and testing
//! assertions.
const mach_port_t kMachPortNull = MACH_PORT_NULL;

//! \brief `MACH_EXCEPTION_CODES` with the correct type for a Mach exception
//!     behavior, `exception_behavior_t`.
//!
//! Signedness problems can occur when ORing `MACH_EXCEPTION_CODES` as a signed
//! integer, because a signed integer overflow results. This constant can be
//! used instead of `MACH_EXCEPTION_CODES` in such cases.
const exception_behavior_t kMachExceptionCodes = MACH_EXCEPTION_CODES;

// Because exception_mask_t is an int and has one bit for each defined
// exception_type_t, it’s reasonable to assume that there cannot be any
// officially-defined exception_type_t values higher than 31.
// kMachExceptionSimulated uses a value well outside this range because it does
// not require a corresponding mask value. Simulated exceptions are delivered to
// the exception handler registered for EXC_CRASH exceptions using
// EXC_MASK_CRASH.

//! \brief An exception type to use for simulated exceptions.
const exception_type_t kMachExceptionSimulated = 'CPsx';

//! \brief Like `mach_thread_self()`, but without the obligation to release the
//!     send right.
//!
//! `mach_thread_self()` returns a send right to the current thread port,
//! incrementing its reference count. This burdens the caller with maintaining
//! this send right, and calling `mach_port_deallocate()` when it is no longer
//! needed. This is burdensome, and is at odds with the normal operation of
//! `mach_task_self()`, which does not increment the task port’s reference count
//! whose result must not be deallocated.
//!
//! Callers can use this function in preference to `mach_thread_self()`. This
//! function returns an extant reference to the current thread’s port without
//! incrementing its reference count.
//!
//! \return The value of `mach_thread_self()` without incrementing its reference
//!     count. The returned port must not be deallocated by
//!     `mach_port_deallocate()`. The returned value is valid as long as the
//!     thread continues to exist as a `pthread_t`.
mach_port_t MachThreadSelf();

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_MACH_MACH_EXTENSIONS_H_
