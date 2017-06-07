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

#include "util/linux/auxiliary_vector.h"

#include <linux/auxvec.h>
#include <unistd.h>

#include "gtest/gtest.h"
#include "test/multiprocess.h"

namespace crashpad {
namespace test {
namespace {

void TestAgainstCloneOrSelf(pid_t pid) {
#if defined(ARCH_CPU_64_BITS)
  AuxiliaryVector<uint64_t> aux;
#else
  AuxiliaryVector<uint32_t> aux;
#endif
  ASSERT_TRUE(aux.Initialize(pid));

  gid_t gid;
  ASSERT_TRUE(aux.GetValue(AT_GID, &gid));
  EXPECT_EQ(gid, getgid());

  int pagesize;
  ASSERT_TRUE(aux.GetValue(AT_PAGESZ, &pagesize));
  EXPECT_EQ(pagesize, getpagesize());
}

TEST(AuxiliaryVector, ReadSelf) {
  TestAgainstCloneOrSelf(getpid());
}

class ReadChildTest : public Multiprocess {
 public:
  ReadChildTest() : Multiprocess() {}
  ~ReadChildTest() {}

 private:
  void MultiprocessParent() override {
    TestAgainstCloneOrSelf(ChildPID());
  }

  void MultiprocessChild() override {
    CheckedReadFileAtEOF(ReadPipeHandle());
  }

  DISALLOW_COPY_AND_ASSIGN(ReadChildTest);
};

TEST(AuxiliaryVector, ReadChild) {
  ReadChildTest test;
  test.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad

