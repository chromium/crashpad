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

#ifndef CRASHPAD_COMPAT_MAC_MACH_O_GETSECT_H_
#define CRASHPAD_COMPAT_MAC_MACH_O_GETSECT_H_

#include_next <mach-o/getsect.h>

#include <AvailabilityMacros.h>

// This file checks the SDK instead of the deployment target. The SDK is correct
// because this file is concerned with providing compile-time declarations,
// which are either present in a specific SDK version or not.

#if MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7

#include <mach-o/loader.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Don’t use a type alias to account for the mach_header/mach_header_64
// difference between the 32-bit and 64-bit versions of getsectiondata() and
// getsegmentdata(). This file should be faithfully equivalent to the native
// SDK, and adding type aliases here would pollute the namespace in a way that
// the native SDK does not.

#if !defined(__LP64__)

uint8_t* getsectiondata(const struct mach_header* mhp,
                        const char* segname,
                        const char* sectname,
                        unsigned long* size);

uint8_t* getsegmentdata(
    const struct mach_header* mhp, const char* segname, unsigned long* size);

#else

uint8_t* getsectiondata(const struct mach_header_64* mhp,
                        const char* segname,
                        const char* sectname,
                        unsigned long* size);

uint8_t* getsegmentdata(
    const struct mach_header_64* mhp, const char* segname, unsigned long* size);

#endif

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7

#endif  // CRASHPAD_COMPAT_MAC_MACH_O_GETSECT_H_
