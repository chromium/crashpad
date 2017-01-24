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

#include "client/capture_context_mac.h"

#include <mach/mach.h>
#include <stdint.h>

#include <algorithm>

#include "build/build_config.h"
#include "gtest/gtest.h"
#include "util/misc/address_sanitizer.h"
#include "util/misc/implicit_cast.h"

namespace crashpad {
namespace test {
namespace {

// If the context structure has fields that tell whether it’s valid, such as
// magic numbers or size fields, sanity-checks those fields for validity with
// fatal gtest assertions. For other fields, where it’s possible to reason about
// their validity based solely on their contents, sanity-checks via nonfatal
// gtest assertions.
void SanityCheckContext(const NativeCPUContext& context) {
#if defined(ARCH_CPU_X86)
  ASSERT_EQ(x86_THREAD_STATE32, context.tsh.flavor);
  ASSERT_EQ(implicit_cast<int>(x86_THREAD_STATE32_COUNT), context.tsh.count);
#elif defined(ARCH_CPU_X86_64)
  ASSERT_EQ(x86_THREAD_STATE64, context.tsh.flavor);
  ASSERT_EQ(implicit_cast<int>(x86_THREAD_STATE64_COUNT), context.tsh.count);
#endif

#if defined(ARCH_CPU_X86_FAMILY)
  // The segment registers are only capable of storing 16-bit quantities, but
  // the context structure provides native integer-width fields for them. Ensure
  // that the high bits are all clear.
  //
  // Many bit positions in the flags register are reserved and will always read
  // a known value. Most reserved bits are always 0, but bit 1 is always 1.
  // Check that the reserved bits are all set to their expected values. Note
  // that the set of reserved bits may be relaxed over time with newer CPUs, and
  // that this test may need to be changed to reflect these developments. The
  // current set of reserved bits are 1, 3, 5, 15, and 22 and higher. See Intel
  // Software Developer’s Manual, Volume 1: Basic Architecture (253665-051),
  // 3.4.3 “EFLAGS Register”, and AMD Architecture Programmer’s Manual, Volume
  // 2: System Programming (24593-3.24), 3.1.6 “RFLAGS Register”.
#if defined(ARCH_CPU_X86)
  EXPECT_EQ(0u, context.uts.ts32.__cs & ~0xffff);
  EXPECT_EQ(0u, context.uts.ts32.__ds & ~0xffff);
  EXPECT_EQ(0u, context.uts.ts32.__es & ~0xffff);
  EXPECT_EQ(0u, context.uts.ts32.__fs & ~0xffff);
  EXPECT_EQ(0u, context.uts.ts32.__gs & ~0xffff);
  EXPECT_EQ(0u, context.uts.ts32.__ss & ~0xffff);
  EXPECT_EQ(2u, context.uts.ts32.__eflags & 0xffc0802a);
#elif defined(ARCH_CPU_X86_64)
  EXPECT_EQ(0u, context.uts.ts64.__cs & ~UINT64_C(0xffff));
  EXPECT_EQ(0u, context.uts.ts64.__fs & ~UINT64_C(0xffff));
  EXPECT_EQ(0u, context.uts.ts64.__gs & ~UINT64_C(0xffff));
  EXPECT_EQ(2u, context.uts.ts64.__rflags & UINT64_C(0xffffffffffc0802a));
#endif
#endif
}

// A CPU-independent function to return the program counter.
uintptr_t ProgramCounterFromContext(const NativeCPUContext& context) {
#if defined(ARCH_CPU_X86)
  return context.uts.ts32.__eip;
#elif defined(ARCH_CPU_X86_64)
  return context.uts.ts64.__rip;
#endif
}

// A CPU-independent function to return the stack pointer.
uintptr_t StackPointerFromContext(const NativeCPUContext& context) {
#if defined(ARCH_CPU_X86)
  return context.uts.ts32.__esp;
#elif defined(ARCH_CPU_X86_64)
  return context.uts.ts64.__rsp;
#endif
}

void TestCaptureContext() {
  NativeCPUContext context_1;
  CaptureContext(&context_1);

  {
    SCOPED_TRACE("context_1");
    ASSERT_NO_FATAL_FAILURE(SanityCheckContext(context_1));
  }

  // The program counter reference value is this function’s address. The
  // captured program counter should be slightly greater than or equal to the
  // reference program counter.
  uintptr_t pc = ProgramCounterFromContext(context_1);

#if !defined(ADDRESS_SANITIZER)
  // AddressSanitizer can cause enough code bloat that the “nearby” check would
  // likely fail.
  const uintptr_t kReferencePC =
      reinterpret_cast<uintptr_t>(TestCaptureContext);
  EXPECT_LT(pc - kReferencePC, 64u);
#endif  // !defined(ADDRESS_SANITIZER)

  // Declare sp and context_2 here because all local variables need to be
  // declared before computing the stack pointer reference value, so that the
  // reference value can be the lowest value possible.
  uintptr_t sp;
  NativeCPUContext context_2;

  // The stack pointer reference value is the lowest address of a local variable
  // in this function. The captured program counter will be slightly less than
  // or equal to the reference stack pointer.
  const uintptr_t kReferenceSP =
      std::min(std::min(reinterpret_cast<uintptr_t>(&context_1),
                        reinterpret_cast<uintptr_t>(&context_2)),
               std::min(reinterpret_cast<uintptr_t>(&pc),
                        reinterpret_cast<uintptr_t>(&sp)));
  sp = StackPointerFromContext(context_1);
  EXPECT_LT(kReferenceSP - sp, 512u);

  // Capture the context again, expecting that the stack pointer stays the same
  // and the program counter increases. Strictly speaking, there’s no guarantee
  // that these conditions will hold, although they do for known compilers even
  // under typical optimization.
  CaptureContext(&context_2);

  {
    SCOPED_TRACE("context_2");
    ASSERT_NO_FATAL_FAILURE(SanityCheckContext(context_2));
  }

  EXPECT_EQ(sp, StackPointerFromContext(context_2));
  EXPECT_GT(ProgramCounterFromContext(context_2), pc);
}

TEST(CaptureContextMac, CaptureContext) {
  ASSERT_NO_FATAL_FAILURE(TestCaptureContext());
}

}  // namespace
}  // namespace test
}  // namespace crashpad
