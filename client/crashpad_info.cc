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

#include "client/crashpad_info.h"

#include "util/stdlib/cxx.h"

#if defined(OS_MACOSX)
#include <mach-o/loader.h>
#endif

#if CXX_LIBRARY_VERSION >= 2011
#include <type_traits>
#endif

namespace {

static const uint32_t kCrashpadInfoVersion = 1;

}  // namespace

namespace crashpad {

#if CXX_LIBRARY_VERSION >= 2011 || DOXYGEN
// In C++11, check that CrashpadInfo has standard layout, which is what is
// actually important.
static_assert(std::is_standard_layout<CrashpadInfo>::value,
              "CrashpadInfo must be standard layout");
#else
// In C++98 (ISO 14882), section 9.5.1 says that a union cannot have a member
// with a non-trivial ctor, copy ctor, dtor, or assignment operator. Use this
// property to ensure that CrashpadInfo remains POD. This doesn’t work for C++11
// because the requirements for unions have been relaxed.
union Compile_Assert {
  CrashpadInfo Compile_Assert__CrashpadInfo_must_be_pod;
};
#endif

// This structure needs to be stored somewhere that is easy to find without
// external information.
//
// It isn’t placed in an unnamed namespace: hopefully, this will catch attempts
// to place multiple copies of this structure into the same module. If that’s
// attempted, and the name of the symbol is the same in each translation unit,
// it will result in a linker error, which is better than having multiple
// structures show up.
//
// This may result in a static module initializer in debug-mode builds, but
// because it’s POD, no code should need to run to initialize this under
// release-mode optimization.
#if defined(OS_MACOSX)

// Put the structure in __DATA,__crashpad_info where it can be easily found
// without having to consult the symbol table. The “used” attribute prevents it
// from being dead-stripped.
__attribute__((section(SEG_DATA ",__crashpad_info"),
               used,
               visibility("hidden")
#if __has_feature(address_sanitizer)
// AddressSanitizer would add a trailing red zone of at least 32 bytes, which
// would be reflected in the size of the custom section. This confuses
// MachOImageReader::GetCrashpadInfo(), which finds that the section’s size
// disagrees with the structure’s size_ field. By specifying an alignment
// greater than the red zone size, the red zone will be suppressed.
               ,
               aligned(64)
#endif
             )) CrashpadInfo g_crashpad_info;

#elif defined(OS_WIN)

// Put the struct in a section name CPADinfo where it can be found without the
// symbol table.
#pragma section("CPADinfo", read, write)
__declspec(allocate("CPADinfo")) CrashpadInfo g_crashpad_info;

#endif

// static
CrashpadInfo* CrashpadInfo::GetCrashpadInfo() {
  return &g_crashpad_info;
}

CrashpadInfo::CrashpadInfo()
    : signature_(kSignature),
      size_(sizeof(*this)),
      version_(kCrashpadInfoVersion),
      crashpad_handler_behavior_(TriState::kUnset),
      system_crash_reporter_forwarding_(TriState::kUnset),
      padding_0_(0),
      simple_annotations_(nullptr)
#if !defined(NDEBUG) && defined(OS_WIN)
      ,
      invalid_read_detection_(0xbadc0de)
#endif
{

}

}  // namespace crashpad
