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

#include "util/test/multiprocess.h"

#include <stdlib.h>
#include <sys/signal.h>
#include <unistd.h>

#include "base/basictypes.h"
#include "gtest/gtest.h"
#include "util/file/fd_io.h"
#include "util/test/errors.h"

namespace {

using namespace crashpad;
using namespace crashpad::test;

class TestMultiprocess final : public Multiprocess {
 public:
  TestMultiprocess() : Multiprocess() {}

  ~TestMultiprocess() {}

 private:
  virtual void MultiprocessParent() override {
    int read_fd = ReadPipeFD();
    char c;
    ssize_t rv = ReadFD(read_fd, &c, 1);
    ASSERT_EQ(1, rv) << ErrnoMessage("read");
    EXPECT_EQ('M', c);

    pid_t pid;
    rv = ReadFD(read_fd, &pid, sizeof(pid));
    ASSERT_EQ(static_cast<ssize_t>(sizeof(pid)), rv) << ErrnoMessage("read");
    EXPECT_EQ(pid, ChildPID());

    int write_fd = WritePipeFD();
    c = 'm';
    rv = WriteFD(write_fd, &c, 1);
    ASSERT_EQ(1, rv) << ErrnoMessage("write");

    // The child will close its end of the pipe and exit. Make sure that the
    // parent sees EOF.
    rv = ReadFD(read_fd, &c, 1);
    ASSERT_EQ(0, rv) << ErrnoMessage("read");
  }

  virtual void MultiprocessChild() override {
    int write_fd = WritePipeFD();

    char c = 'M';
    ssize_t rv = WriteFD(write_fd, &c, 1);
    ASSERT_EQ(1, rv) << ErrnoMessage("write");

    pid_t pid = getpid();
    rv = WriteFD(write_fd, &pid, sizeof(pid));
    ASSERT_EQ(static_cast<ssize_t>(sizeof(pid)), rv) << ErrnoMessage("write");

    int read_fd = ReadPipeFD();
    rv = ReadFD(read_fd, &c, 1);
    ASSERT_EQ(1, rv) << ErrnoMessage("read");
    EXPECT_EQ('m', c);
  }

  DISALLOW_COPY_AND_ASSIGN(TestMultiprocess);
};

TEST(Multiprocess, Multiprocess) {
  TestMultiprocess multiprocess;
  multiprocess.Run();
}

class TestMultiprocessUnclean final : public Multiprocess {
 public:
  enum TerminationType {
    kExitSuccess = 0,
    kExitFailure,
    kExit2,
    kAbort,
  };

  explicit TestMultiprocessUnclean(TerminationType type)
      : Multiprocess(),
        type_(type) {
    if (type_ == kAbort) {
      SetExpectedChildTermination(kTerminationSignal, SIGABRT);
    } else {
      SetExpectedChildTermination(kTerminationNormal, ExitCode());
    }
  }

  ~TestMultiprocessUnclean() {}

 private:
  int ExitCode() const {
    return type_;
  }

  virtual void MultiprocessParent() override {
  }

  virtual void MultiprocessChild() override {
    if (type_ == kAbort) {
      abort();
    } else {
      _exit(ExitCode());
    }
  }

  TerminationType type_;

  DISALLOW_COPY_AND_ASSIGN(TestMultiprocessUnclean);
};

TEST(Multiprocess, MultiprocessSuccessfulExit) {
  TestMultiprocessUnclean multiprocess(TestMultiprocessUnclean::kExitSuccess);
  multiprocess.Run();
}

TEST(Multiprocess, MultiprocessUnsuccessfulExit) {
  TestMultiprocessUnclean multiprocess(TestMultiprocessUnclean::kExitFailure);
  multiprocess.Run();
}

TEST(Multiprocess, MultiprocessExit2) {
  TestMultiprocessUnclean multiprocess(TestMultiprocessUnclean::kExit2);
  multiprocess.Run();
}

TEST(Multiprocess, MultiprocessAbortSignal) {
  TestMultiprocessUnclean multiprocess(TestMultiprocessUnclean::kAbort);
  multiprocess.Run();
}

}  // namespace
