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

#include "util/misc/address_sanitizer.h"
#include "util/misc/from_pointer_cast.h"
#include "util/stdlib/cxx.h"

#if defined(OS_MACOSX)
#include <mach-o/loader.h>
#endif

#if CXX_LIBRARY_VERSION >= 2011
#include <type_traits>
#endif

namespace {

constexpr uint32_t kCrashpadInfoVersion = 1;

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
#if defined(OS_POSIX)
__attribute__((

    // Put the structure in a well-known section name where it can be easily
    // found without having to consult the symbol table.
#if defined(OS_MACOSX)
    section(SEG_DATA ",crashpad_info"),
#elif defined(OS_LINUX) || defined(OS_ANDROID)
    section("crashpad_info"),
#else  // !defined(OS_MACOSX) && !defined(OS_LINUX) && !defined(OS_ANDROID)
#error Port
#endif  // !defined(OS_MACOSX) && !defined(OS_LINUX) && !defined(OS_ANDROID)

#if defined(ADDRESS_SANITIZER)
    // AddressSanitizer would add a trailing red zone of at least 32 bytes,
    // which would be reflected in the size of the custom section. This confuses
    // MachOImageReader::GetCrashpadInfo(), which finds that the section’s size
    // disagrees with the structure’s size_ field. By specifying an alignment
    // greater than the red zone size, the red zone will be suppressed.
    aligned(64),
#endif  // defined(ADDRESS_SANITIZER)

    // The “used” attribute prevents the structure from being dead-stripped.
    used,

    // There’s no need to expose this as a public symbol from the symbol table.
    // All accesses from the outside can locate the well-known section name.
    visibility("hidden")))

#elif defined(OS_WIN)

// Put the struct in a section name CPADinfo where it can be found without the
// symbol table.
#pragma section("CPADinfo", read, write)
__declspec(allocate("CPADinfo"))

#else  // !defined(OS_POSIX) && !defined(OS_WIN)
#error Port
#endif  // !defined(OS_POSIX) && !defined(OS_WIN)

CrashpadInfo g_crashpad_info;

// static
CrashpadInfo* CrashpadInfo::GetCrashpadInfo() {
  return &g_crashpad_info;
}

CrashpadInfo::CrashpadInfo()
    : signature_(kSignature),
      size_(sizeof(*this)),
      version_(kCrashpadInfoVersion),
      indirectly_referenced_memory_cap_(0),
      padding_0_(0),
      crashpad_handler_behavior_(TriState::kUnset),
      system_crash_reporter_forwarding_(TriState::kUnset),
      gather_indirectly_referenced_memory_(TriState::kUnset),
      padding_1_(0),
      extra_memory_ranges_(nullptr),
      simple_annotations_(nullptr),
      user_data_minidump_stream_head_(nullptr)
#if !defined(NDEBUG) && defined(OS_WIN)
      ,
      invalid_read_detection_(0xbadc0de)
#endif
{
}

void CrashpadInfo::AddUserDataMinidumpStream(uint32_t stream_type,
                                             const void* data,
                                             size_t size) {
  auto to_be_added = new internal::UserDataMinidumpStreamListEntry();
  to_be_added->next =
      FromPointerCast<uint64_t>(user_data_minidump_stream_head_);
  to_be_added->stream_type = stream_type;
  to_be_added->base_address = FromPointerCast<uint64_t>(data);
  to_be_added->size = base::checked_cast<uint64_t>(size);
  user_data_minidump_stream_head_ = to_be_added;
}

}  // namespace crashpad
