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

#include <mach-o/getsect.h>

// This is only necessary when building code that might run on systems earlier
// than 10.7. When building for 10.7 or later, getsectiondata() and
// getsegmentdata() are always present in libmacho and made available through
// libSystem. When building for earlier systems, custom definitions of
// these functions are needed.
//
// This file checks the deployment target instead of the SDK. The deployment
// target is correct because it identifies the earliest possible system that
// the code being compiled is expected to run on.

#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_7

#include <dlfcn.h>
#include <stddef.h>

#include "third_party/apple_cctools/cctools/include/mach-o/getsect.h"

namespace {

// Returns a dlopen() handle to the same library that provides the
// getsectbyname() function. getsectbyname() is always present in libmacho.
// getsectiondata() and getsegmentdata() are not always present, but when they
// are, they’re in the same library as getsectbyname(). If the library cannot
// be found or a handle to it cannot be returned, returns nullptr.
void* SystemLibMachOHandle() {
  Dl_info info;
  if (!dladdr(reinterpret_cast<void*>(getsectbyname), &info)) {
    return nullptr;
  }
  return dlopen(info.dli_fname, RTLD_LAZY | RTLD_LOCAL | RTLD_NOLOAD);
}

// Returns a function pointer to a function in libmacho based on a lookup of
// that function by symbol name. Returns nullptr if libmacho cannot be found or
// opened, or if the named symbol cannot be found in libmacho.
void* LookUpSystemLibMachOSymbol(const char* symbol) {
  static void* dl_handle = SystemLibMachOHandle();
  if (!dl_handle) {
    return nullptr;
  }
  return dlsym(dl_handle, symbol);
}

#ifndef __LP64__
using MachHeader = mach_header;
#else
using MachHeader = mach_header_64;
#endif

using GetSectionDataType =
    uint8_t*(*)(const MachHeader*, const char*, const char*, unsigned long*);
using GetSegmentDataType =
    uint8_t*(*)(const MachHeader*, const char*, unsigned long*);

}  // namespace

extern "C" {

// These implementations look up their functions in libmacho at run time. If
// the system libmacho provides these functions as it normally does on Mac OS X
// 10.7 and later, the system’s versions are used directly. Otherwise, the
// versions in third_party/apple_cctools are used, which are actually just
// copies of the system’s functions.

uint8_t* getsectiondata(const MachHeader* mhp,
                        const char* segname,
                        const char* sectname,
                        unsigned long* size) {
  static GetSectionDataType system_getsectiondata =
      reinterpret_cast<GetSectionDataType>(
          LookUpSystemLibMachOSymbol("getsectiondata"));
  if (system_getsectiondata) {
    return system_getsectiondata(mhp, segname, sectname, size);
  }
  return crashpad_getsectiondata(mhp, segname, sectname, size);
}

uint8_t* getsegmentdata(
    const MachHeader* mhp, const char* segname, unsigned long* size) {
  static GetSegmentDataType system_getsegmentdata =
      reinterpret_cast<GetSegmentDataType>(
          LookUpSystemLibMachOSymbol("getsegmentdata"));
  if (system_getsegmentdata) {
    return system_getsegmentdata(mhp, segname, size);
  }
  return crashpad_getsegmentdata(mhp, segname, size);
}

}  // extern "C"

#endif  // MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_7
