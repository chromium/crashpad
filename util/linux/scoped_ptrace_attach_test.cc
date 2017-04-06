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

#include "util/linux/scoped_ptrace_attach.h"

#include <sys/ptrace.h>
#include <unistd.h>

#include "gtest/gtest.h"
#include "test/errors.h"
#include "test/multiprocess.h"
#include "util/file/file_io.h"

namespace crashpad {
namespace test {
namespace {

const long kWord = 42;

class AttachToChildTest : public Multiprocess {
 public:
  AttachToChildTest() : Multiprocess() {}
  ~AttachToChildTest() {}

 private:
  void MultiprocessParent() override {
    pid_t pid = ChildPID();
    ScopedPtraceAttach attachment;
    ASSERT_EQ(attachment.ResetAttach(pid), true);
    EXPECT_EQ(ptrace(PTRACE_PEEKDATA, pid, &kWord, nullptr), kWord);
    attachment.Reset();
    EXPECT_EQ(ptrace(PTRACE_PEEKDATA, pid, &kWord, nullptr), -1);
  }

  void MultiprocessChild() override {
    CheckedReadFileAtEOF(ReadPipeHandle());
  }

  DISALLOW_COPY_AND_ASSIGN(AttachToChildTest);
};

TEST(ScopedPtraceAttach, AttachChild) {
  AttachToChildTest test;
  test.Run();
}

class AttachToParentResetTest : public Multiprocess {
 public:
  AttachToParentResetTest() : Multiprocess() {}
  ~AttachToParentResetTest() {}

 private:
  void MultiprocessParent() override {
    CheckedReadFileAtEOF(ReadPipeHandle());
  }

  void MultiprocessChild() override {
    pid_t pid = getppid();
    ScopedPtraceAttach attachment;
    ASSERT_EQ(attachment.ResetAttach(pid), true);
    EXPECT_EQ(ptrace(PTRACE_PEEKDATA, pid, &kWord, nullptr), kWord);
    attachment.Reset();
    EXPECT_EQ(ptrace(PTRACE_PEEKDATA, pid, &kWord, nullptr), -1);
  }

  DISALLOW_COPY_AND_ASSIGN(AttachToParentResetTest);
};

TEST(ScopedPtraceAttach, AttachParentReset) {
  AttachToParentResetTest test;
  test.Run();
}

class AttachToParentDestructorTest : public Multiprocess {
 public:
  AttachToParentDestructorTest() : Multiprocess() {}
  ~AttachToParentDestructorTest() {}

 private:
  void MultiprocessParent() override {
    CheckedReadFileAtEOF(ReadPipeHandle());
  }

  void MultiprocessChild() override {
    pid_t pid = getppid();
    {
      ScopedPtraceAttach attachment;
      ASSERT_EQ(attachment.ResetAttach(pid), true);
      EXPECT_EQ(ptrace(PTRACE_PEEKDATA, pid, &kWord, nullptr), kWord);
    }
    EXPECT_EQ(ptrace(PTRACE_PEEKDATA, pid, &kWord, nullptr), -1);
  }

  DISALLOW_COPY_AND_ASSIGN(AttachToParentDestructorTest);
};

TEST(ScopedPtraceAttach, AttachParentDestructor) {
  AttachToParentDestructorTest test;
  test.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
