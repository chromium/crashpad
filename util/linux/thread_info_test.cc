// Copyright 2017 The Crashpad Authors. All rights reserved.
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

#include "util/linux/thread_info.h"

#include "build/build_config.h"
#include "gtest/gtest.h"
#include "test/multiprocess.h"
#include "util/file/file_io.h"
#include "util/misc/from_pointer_cast.h"

namespace crashpad {
namespace test {
namespace {

class SameBitnessTest : public Multiprocess {
 public:
  SameBitnessTest() : Multiprocess() {}
  ~SameBitnessTest() {}

 private:
  void MultiprocessParent() override {
    LinuxVMAddress expected_tls;
    CheckedReadFileExactly(
        ReadPipeHandle(), &expected_tls, sizeof(expected_tls));

    ThreadInfo thread_info;
    ASSERT_TRUE(thread_info.Initialize(ChildPID()));

#if defined(ARCH_CPU_64_BITS)
    const bool am_64_bit = true;
#else
    const bool am_64_bit = false;
#endif  // ARCH_CPU_64_BITS

    EXPECT_EQ(thread_info.Is64Bit(), am_64_bit);

    ThreadContext thread_context;
    thread_info.GetGeneralPurposeRegisters(&thread_context);

#if defined(ARCH_CPU_X86_64)
    EXPECT_EQ(thread_context.t64.fs_base, expected_tls);
#endif  // ARCH_CPU_X86_64

    FloatContext float_context;
    EXPECT_TRUE(thread_info.GetFloatingPointRegisters(&float_context));

    LinuxVMAddress tls_address;
    ASSERT_TRUE(thread_info.GetThreadArea(&tls_address));
    EXPECT_EQ(tls_address, expected_tls);
  }

  void MultiprocessChild() override {
    LinuxVMAddress expected_tls;
#if defined(ARCH_CPU_ARMEL)
    // 0xffff0fe0 is the address of the kernel user helper __kuser_get_tls().
    auto kuser_get_tls = reinterpret_cast<void* (*)()>(0xffff0fe0);
    expected_tls = FromPointerCast<LinuxVMAddress>(kuser_get_tls());
#elif defined(ARCH_CPU_ARM64)
    // Linux/aarch64 places the tls address in system register tpidr_el0.
    asm("mrs %0, tpidr_el0" : "=r"(expected_tls));
#elif defined(ARCH_CPU_X86)
    uint32_t expected_tls_32;
    asm("movl %%gs:0x0, %0" : "=r"(expected_tls_32));
    expected_tls = expected_tls_32;
#elif defined(ARCH_CPU_X86_64)
    asm("movq %%fs:0x0, %0" : "=r"(expected_tls));
#else
#error Port.
#endif  // ARCH_CPU_ARMEL

    CheckedWriteFile(WritePipeHandle(), &expected_tls, sizeof(expected_tls));

    CheckedReadFileAtEOF(ReadPipeHandle());
  }

  DISALLOW_COPY_AND_ASSIGN(SameBitnessTest);
};

TEST(ThreadInfo, SameBitness) {
  SameBitnessTest test;
  test.Run();
}

// TODO(jperaza): Test against a process with different bitness.

}  // namespace
}  // namespace test
}  // namespace crashpad
