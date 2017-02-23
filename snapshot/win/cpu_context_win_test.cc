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

#include "base/macros.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "test/hex_string.h"
#include "snapshot/cpu_context.h"

namespace crashpad {
namespace test {
namespace {

template <typename T>
void TestInitializeX86Context() {
  T context = {0};
  context.ContextFlags = WOW64_CONTEXT_INTEGER |
                         WOW64_CONTEXT_DEBUG_REGISTERS |
                         WOW64_CONTEXT_EXTENDED_REGISTERS;
  context.Eax = 1;
  context.Dr0 = 3;
  context.ExtendedRegisters[4] = 2;  // FTW

  // Test the simple case, where everything in the CPUContextX86 argument is set
  // directly from the supplied thread, float, and debug state parameters.
  {
    CPUContextX86 cpu_context_x86 = {};
    InitializeX86Context(context, &cpu_context_x86);
    EXPECT_EQ(1u, cpu_context_x86.eax);
    EXPECT_EQ(2u, cpu_context_x86.fxsave.ftw);
    EXPECT_EQ(3u, cpu_context_x86.dr0);
  }
}

template <typename T>
void TestInitializeX86Context_FsaveWithoutFxsave() {
  T context = {0};
  context.ContextFlags = WOW64_CONTEXT_INTEGER |
                         WOW64_CONTEXT_FLOATING_POINT |
                         WOW64_CONTEXT_DEBUG_REGISTERS;
  context.Eax = 1;

  // In fields that are wider than they need to be, set the high bits to ensure
  // that they’re masked off appropriately in the output.
  context.FloatSave.ControlWord = 0xffff027f;
  context.FloatSave.StatusWord = 0xffff0004;
  context.FloatSave.TagWord = 0xffffa9ff;
  context.FloatSave.ErrorOffset = 0x01234567;
  context.FloatSave.ErrorSelector = 0x0bad0003;
  context.FloatSave.DataOffset = 0x89abcdef;
  context.FloatSave.DataSelector = 0xffff0007;
  context.FloatSave.RegisterArea[77] = 0x80;
  context.FloatSave.RegisterArea[78] = 0xff;
  context.FloatSave.RegisterArea[79] = 0x7f;

  context.Dr0 = 3;

  {
    CPUContextX86 cpu_context_x86 = {};
    InitializeX86Context(context, &cpu_context_x86);

    EXPECT_EQ(1u, cpu_context_x86.eax);

    EXPECT_EQ(0x027f, cpu_context_x86.fxsave.fcw);
    EXPECT_EQ(0x0004, cpu_context_x86.fxsave.fsw);
    EXPECT_EQ(0x00f0, cpu_context_x86.fxsave.ftw);
    EXPECT_EQ(0x0bad, cpu_context_x86.fxsave.fop);
    EXPECT_EQ(0x01234567, cpu_context_x86.fxsave.fpu_ip);
    EXPECT_EQ(0x0003, cpu_context_x86.fxsave.fpu_cs);
    EXPECT_EQ(0x89abcdef, cpu_context_x86.fxsave.fpu_dp);
    EXPECT_EQ(0x0007, cpu_context_x86.fxsave.fpu_ds);
    for (size_t st_mm = 0; st_mm < 7; ++st_mm) {
      EXPECT_EQ(
          std::string(arraysize(cpu_context_x86.fxsave.st_mm[st_mm].st) * 2,
                      '0'),
          BytesToHexString(cpu_context_x86.fxsave.st_mm[st_mm].st,
                           arraysize(cpu_context_x86.fxsave.st_mm[st_mm].st)))
          << "st_mm " << st_mm;
    }
    EXPECT_EQ("0000000000000080ff7f",
              BytesToHexString(cpu_context_x86.fxsave.st_mm[7].st,
                               arraysize(cpu_context_x86.fxsave.st_mm[7].st)));

    EXPECT_EQ(3u, cpu_context_x86.dr0);
  }
}

#if defined(ARCH_CPU_X86_FAMILY)

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

#endif  // ARCH_CPU_X86_64

TEST(CPUContextWin, InitializeX86Context) {
#if defined(ARCH_CPU_X86)
  TestInitializeX86Context<CONTEXT>();
#else  // ARCH_CPU_X86
  TestInitializeX86Context<WOW64_CONTEXT>();
#endif  // ARCH_CPU_X86
}

TEST(CPUContextWin, InitializeX86Context_FsaveWithoutFxsave) {
#if defined(ARCH_CPU_X86)
  TestInitializeX86Context_FsaveWithoutFxsave<CONTEXT>();
#else  // ARCH_CPU_X86
  TestInitializeX86Context_FsaveWithoutFxsave<WOW64_CONTEXT>();
#endif  // ARCH_CPU_X86
}

#endif  // ARCH_CPU_X86_FAMILY

}  // namespace
}  // namespace test
}  // namespace crashpad
