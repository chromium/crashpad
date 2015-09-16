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

#include "snapshot/win/cpu_context_win.h"

#include <windows.h>

#include "build/build_config.h"
#include "gtest/gtest.h"
#include "snapshot/cpu_context.h"

namespace crashpad {
namespace test {
namespace {

#if defined(ARCH_CPU_X86_64)

TEST(CPUContextWin, InitializeX64Context) {
  CONTEXT context = {0};
  context.Rax = 10;
  context.FltSave.TagWord = 11;
  context.Dr0 = 12;
  context.ContextFlags =
      CONTEXT_INTEGER | CONTEXT_FLOATING_POINT | CONTEXT_DEBUG_REGISTERS;

  // Test the simple case, where everything in the CPUContextX86_64 argument is
  // set directly from the supplied thread, float, and debug state parameters.
  {
    CPUContextX86_64 cpu_context_x86_64 = {};
    InitializeX64Context(context, &cpu_context_x86_64);
    EXPECT_EQ(10u, cpu_context_x86_64.rax);
    EXPECT_EQ(11u, cpu_context_x86_64.fxsave.ftw);
    EXPECT_EQ(12u, cpu_context_x86_64.dr0);
  }
}

#else

TEST(CPUContextWin, InitializeX86Context) {
  CONTEXT context = {0};
  context.ContextFlags =
      CONTEXT_INTEGER | CONTEXT_EXTENDED_REGISTERS | CONTEXT_DEBUG_REGISTERS;
  context.Eax = 1;
  context.ExtendedRegisters[4] = 2;  // FTW.
  context.Dr0 = 3;

  // Test the simple case, where everything in the CPUContextX86 argument is
  // set directly from the supplied thread, float, and debug state parameters.
  {
    CPUContextX86 cpu_context_x86 = {};
    InitializeX86Context(context, &cpu_context_x86);
    EXPECT_EQ(1u, cpu_context_x86.eax);
    EXPECT_EQ(2u, cpu_context_x86.fxsave.ftw);
    EXPECT_EQ(3u, cpu_context_x86.dr0);
  }
}

#endif  // ARCH_CPU_X86_64

}  // namespace
}  // namespace test
}  // namespace crashpad
