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

#include "util/linux/ptrace_requests.h"

#include <elf.h>
#include <sys/ptrace.h>
#include <sys/uio.h>

#include "gtest/gtest.h"
#include "test/errors.h"
#include "test/multiprocess.h"
#include "util/file/file_io.h"
#include "util/linux/scoped_ptrace_attach.h"

namespace crashpad {
namespace test {
namespace {

class SameBitnessTest : public Multiprocess {
 public:
  SameBitnessTest() : Multiprocess() {}
  ~SameBitnessTest() {}

 private:
  void MultiprocessParent() override {
    ScopedPtraceAttach attachment;
    ASSERT_TRUE(attachment.ResetAttach(ChildPID()));

#if defined(ARCH_CPU_64_BITS)
    auto expected = GetRegistersResult::k64Bit;
#else
    auto expected = GetRegistersResult::k32Bit;
#endif

    ThreadContext thread_context;
    EXPECT_EQ(GetGeneralPurposeRegisters(ChildPID(), &thread_context),
              expected);

    FloatContext float_context;
    EXPECT_EQ(GetFloatingPointRegisters(ChildPID(), &float_context), expected);
  }

  void MultiprocessChild() override { CheckedReadFileAtEOF(ReadPipeHandle()); }

  DISALLOW_COPY_AND_ASSIGN(SameBitnessTest);
};

TEST(PtraceRequests, SameBitness) {
  SameBitnessTest test;
  test.Run();
}

class UnattachedTest : public Multiprocess {
 public:
  UnattachedTest() : Multiprocess() {}
  ~UnattachedTest() {}

 private:
  void MultiprocessParent() override {
    ThreadContext thread_context;
    EXPECT_EQ(GetGeneralPurposeRegisters(ChildPID(), &thread_context),
              GetRegistersResult::kError);

    FloatContext float_context;
    EXPECT_EQ(GetFloatingPointRegisters(ChildPID(), &float_context),
              GetRegistersResult::kError);
  }

  void MultiprocessChild() override { CheckedReadFileAtEOF(ReadPipeHandle()); }

  DISALLOW_COPY_AND_ASSIGN(UnattachedTest);
};

TEST(PtraceRequests, UnattachedRequestsReturnErrors) {
  UnattachedTest test;
  test.Run();
}

// TODO(jperaza): Test against a process with different bitness.

}  // namespace
}  // namespace test
}  // namespace crashpad
