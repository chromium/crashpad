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

#include "util/mach/exception_types.h"

#include <kern/exc_resource.h>
#include <signal.h>
#include <stdint.h>
#include <sys/types.h>
#include <unistd.h>

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "gtest/gtest.h"
#include "util/mac/mac_util.h"
#include "util/mach/mach_extensions.h"

namespace crashpad {
namespace test {
namespace {

TEST(ExceptionTypes, ExcCrashRecoverOriginalException) {
  struct TestData {
    mach_exception_code_t code_0;
    exception_type_t exception;
    mach_exception_code_t original_code_0;
    int signal;
  };
  const TestData kTestData[] = {
      {0xb100001, EXC_BAD_ACCESS, KERN_INVALID_ADDRESS, SIGSEGV},
      {0xb100002, EXC_BAD_ACCESS, KERN_PROTECTION_FAILURE, SIGSEGV},
      {0xa100002, EXC_BAD_ACCESS, KERN_PROTECTION_FAILURE, SIGBUS},
      {0x4200001, EXC_BAD_INSTRUCTION, 1, SIGILL},
      {0x8300001, EXC_ARITHMETIC, 1, SIGFPE},
      {0x5600002, EXC_BREAKPOINT, 2, SIGTRAP},
      {0x3000000, 0, 0, SIGQUIT},
      {0x6000000, 0, 0, SIGABRT},
      {0xc000000, 0, 0, SIGSYS},
      {0, 0, 0, 0},
  };

  for (size_t index = 0; index < arraysize(kTestData); ++index) {
    const TestData& test_data = kTestData[index];
    SCOPED_TRACE(base::StringPrintf(
        "index %zu, code_0 0x%llx", index, test_data.code_0));

    mach_exception_code_t original_code_0;
    int signal;
    exception_type_t exception = ExcCrashRecoverOriginalException(
        test_data.code_0, &original_code_0, &signal);

    EXPECT_EQ(test_data.exception, exception);
    EXPECT_EQ(test_data.original_code_0, original_code_0);
    EXPECT_EQ(test_data.signal, signal);
  }

  // Now make sure that ExcCrashRecoverOriginalException() properly ignores
  // optional arguments.
  static_assert(arraysize(kTestData) >= 1, "must have something to test");
  const TestData& test_data = kTestData[0];
  EXPECT_EQ(
      test_data.exception,
      ExcCrashRecoverOriginalException(test_data.code_0, nullptr, nullptr));

  mach_exception_code_t original_code_0;
  EXPECT_EQ(test_data.exception,
            ExcCrashRecoverOriginalException(
                test_data.code_0, &original_code_0, nullptr));
  EXPECT_EQ(test_data.original_code_0, original_code_0);

  int signal;
  EXPECT_EQ(
      test_data.exception,
      ExcCrashRecoverOriginalException(test_data.code_0, nullptr, &signal));
  EXPECT_EQ(test_data.signal, signal);
}

// These macros come from the #ifdef KERNEL section of <kern/exc_resource.h>:
// 10.10 xnu-2782.1.97/osfmk/kern/exc_resource.h.
#define EXC_RESOURCE_ENCODE_TYPE(code, type) \
  ((code) |= ((static_cast<uint64_t>(type) & 0x7ull) << 61))
#define EXC_RESOURCE_ENCODE_FLAVOR(code, flavor) \
  ((code) |= ((static_cast<uint64_t>(flavor) & 0x7ull) << 58))

TEST(ExceptionTypes, IsExceptionNonfatalResource) {
  const pid_t pid = getpid();

  mach_exception_code_t code = 0;
  EXC_RESOURCE_ENCODE_TYPE(code, RESOURCE_TYPE_CPU);
  EXC_RESOURCE_ENCODE_FLAVOR(code, FLAVOR_CPU_MONITOR);
  EXPECT_TRUE(IsExceptionNonfatalResource(EXC_RESOURCE, code, pid));

  if (MacOSXMinorVersion() >= 10) {
    // FLAVOR_CPU_MONITOR_FATAL was introduced in Mac OS X 10.10.
    code = 0;
    EXC_RESOURCE_ENCODE_TYPE(code, RESOURCE_TYPE_CPU);
    EXC_RESOURCE_ENCODE_FLAVOR(code, FLAVOR_CPU_MONITOR_FATAL);
    EXPECT_FALSE(IsExceptionNonfatalResource(EXC_RESOURCE, code, pid));
  }

  // This assumes that WAKEMON_MAKE_FATAL is not set for this process. The
  // default is for WAKEMON_MAKE_FATAL to not be set, there’s no public API to
  // enable it, and nothing in this process should have enabled it.
  code = 0;
  EXC_RESOURCE_ENCODE_TYPE(code, RESOURCE_TYPE_WAKEUPS);
  EXC_RESOURCE_ENCODE_FLAVOR(code, FLAVOR_WAKEUPS_MONITOR);
  EXPECT_TRUE(IsExceptionNonfatalResource(EXC_RESOURCE, code, pid));

  code = 0;
  EXC_RESOURCE_ENCODE_TYPE(code, RESOURCE_TYPE_MEMORY);
  EXC_RESOURCE_ENCODE_FLAVOR(code, FLAVOR_HIGH_WATERMARK);
  EXPECT_TRUE(IsExceptionNonfatalResource(EXC_RESOURCE, code, pid));

  // Non-EXC_RESOURCE exceptions should never be considered nonfatal resource
  // exceptions, because they aren’t resource exceptions at all.
  EXPECT_FALSE(IsExceptionNonfatalResource(EXC_CRASH, 0xb100001, pid));
  EXPECT_FALSE(IsExceptionNonfatalResource(EXC_CRASH, 0x0b00000, pid));
  EXPECT_FALSE(IsExceptionNonfatalResource(EXC_CRASH, 0x6000000, pid));
  EXPECT_FALSE(
      IsExceptionNonfatalResource(EXC_BAD_ACCESS, KERN_INVALID_ADDRESS, pid));
  EXPECT_FALSE(IsExceptionNonfatalResource(EXC_BAD_INSTRUCTION, 1, pid));
  EXPECT_FALSE(IsExceptionNonfatalResource(EXC_ARITHMETIC, 1, pid));
  EXPECT_FALSE(IsExceptionNonfatalResource(EXC_BREAKPOINT, 2, pid));
  EXPECT_FALSE(IsExceptionNonfatalResource(0, 0, pid));
  EXPECT_FALSE(IsExceptionNonfatalResource(kMachExceptionSimulated, 0, pid));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
