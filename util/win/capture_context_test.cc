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

#include "util/win/capture_context.h"

#include <stdint.h>
#include <sys/types.h>

#include <algorithm>

#include "base/macros.h"
#include "build/build_config.h"
#include "gtest/gtest.h"

namespace crashpad {
namespace test {
namespace {

// If the context structure has fields that tell whether it’s valid, such as
// magic numbers or size fields, sanity-checks those fields for validity with
// fatal gtest assertions. For other fields, where it’s possible to reason about
// their validity based solely on their contents, sanity-checks via nonfatal
// gtest assertions.
void SanityCheckContext(const CONTEXT& context) {
#if defined(ARCH_CPU_X86)
  constexpr uint32_t must_have = CONTEXT_i386 |
                                 CONTEXT_CONTROL |
                                 CONTEXT_INTEGER |
                                 CONTEXT_SEGMENTS |
                                 CONTEXT_FLOATING_POINT;
  ASSERT_EQ(context.ContextFlags & must_have, must_have);
  constexpr uint32_t may_have = CONTEXT_EXTENDED_REGISTERS;
  ASSERT_EQ(context.ContextFlags & ~(must_have | may_have), 0);
#elif defined(ARCH_CPU_X86_64)
  ASSERT_EQ(context.ContextFlags,
            CONTEXT_AMD64 | CONTEXT_CONTROL | CONTEXT_INTEGER |
                CONTEXT_SEGMENTS | CONTEXT_FLOATING_POINT);
#endif

#if defined(ARCH_CPU_X86_FAMILY)
  // Many bit positions in the flags register are reserved and will always read
  // a known value. Most reserved bits are always 0, but bit 1 is always 1.
  // Check that the reserved bits are all set to their expected values. Note
  // that the set of reserved bits may be relaxed over time with newer CPUs, and
  // that this test may need to be changed to reflect these developments. The
  // current set of reserved bits are 1, 3, 5, 15, and 22 and higher. See Intel
  // Software Developer’s Manual, Volume 1: Basic Architecture (253665-055),
  // 3.4.3 “EFLAGS Register”, and AMD Architecture Programmer’s Manual, Volume
  // 2: System Programming (24593-3.25), 3.1.6 “RFLAGS Register”.
  EXPECT_EQ(context.EFlags & 0xffc0802a, 2u);

  // CaptureContext() doesn’t capture debug registers, so make sure they read 0.
  EXPECT_EQ(context.Dr0, 0);
  EXPECT_EQ(context.Dr1, 0);
  EXPECT_EQ(context.Dr2, 0);
  EXPECT_EQ(context.Dr3, 0);
  EXPECT_EQ(context.Dr6, 0);
  EXPECT_EQ(context.Dr7, 0);
#endif

#if defined(ARCH_CPU_X86)
  // fxsave doesn’t write these bytes.
  for (size_t i = 464; i < arraysize(context.ExtendedRegisters); ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(context.ExtendedRegisters[i], 0);
  }
#elif defined(ARCH_CPU_X86_64)
  // mxcsr shows up twice in the context structure. Make sure the values are
  // identical.
  EXPECT_EQ(context.FltSave.MxCsr, context.MxCsr);

  // fxsave doesn’t write these bytes.
  for (size_t i = 0; i < arraysize(context.FltSave.Reserved4); ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(context.FltSave.Reserved4[i], 0);
  }

  // CaptureContext() doesn’t use these fields.
  EXPECT_EQ(context.P1Home, 0);
  EXPECT_EQ(context.P2Home, 0);
  EXPECT_EQ(context.P3Home, 0);
  EXPECT_EQ(context.P4Home, 0);
  EXPECT_EQ(context.P5Home, 0);
  EXPECT_EQ(context.P6Home, 0);
  for (size_t i = 0; i < arraysize(context.VectorRegister); ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(context.VectorRegister[i].Low, 0);
    EXPECT_EQ(context.VectorRegister[i].High, 0);
  }
  EXPECT_EQ(context.VectorControl, 0);
  EXPECT_EQ(context.DebugControl, 0);
  EXPECT_EQ(context.LastBranchToRip, 0);
  EXPECT_EQ(context.LastBranchFromRip, 0);
  EXPECT_EQ(context.LastExceptionToRip, 0);
  EXPECT_EQ(context.LastExceptionFromRip, 0);
#endif
}

// A CPU-independent function to return the program counter.
uintptr_t ProgramCounterFromContext(const CONTEXT& context) {
#if defined(ARCH_CPU_X86)
  return context.Eip;
#elif defined(ARCH_CPU_X86_64)
  return context.Rip;
#endif
}

// A CPU-independent function to return the stack pointer.
uintptr_t StackPointerFromContext(const CONTEXT& context) {
#if defined(ARCH_CPU_X86)
  return context.Esp;
#elif defined(ARCH_CPU_X86_64)
  return context.Rsp;
#endif
}

void TestCaptureContext() {
  CONTEXT context_1;
  CaptureContext(&context_1);

  {
    SCOPED_TRACE("context_1");
    ASSERT_NO_FATAL_FAILURE(SanityCheckContext(context_1));
  }

  // The program counter reference value is this function’s address. The
  // captured program counter should be slightly greater than or equal to the
  // reference program counter.
  uintptr_t pc = ProgramCounterFromContext(context_1);

  // Declare sp and context_2 here because all local variables need to be
  // declared before computing the stack pointer reference value, so that the
  // reference value can be the lowest value possible.
  uintptr_t sp;
  CONTEXT context_2;

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

  EXPECT_EQ(StackPointerFromContext(context_2), sp);
  EXPECT_GT(ProgramCounterFromContext(context_2), pc);
}

TEST(CaptureContextWin, CaptureContext) {
  ASSERT_NO_FATAL_FAILURE(TestCaptureContext());
}

}  // namespace
}  // namespace test
}  // namespace crashpad
