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

namespace crashpad {
namespace test {
namespace {

class TestMultiprocess final : public Multiprocess {
 public:
  TestMultiprocess() : Multiprocess() {}

  ~TestMultiprocess() {}

 private:
  // Multiprocess:

  virtual void MultiprocessParent() override {
    int read_fd = ReadPipeFD();
    char c;
    CheckedReadFD(read_fd, &c, 1);
    EXPECT_EQ('M', c);

    pid_t pid;
    CheckedReadFD(read_fd, &pid, sizeof(pid));
    EXPECT_EQ(pid, ChildPID());

    c = 'm';
    CheckedWriteFD(WritePipeFD(), &c, 1);

    // The child will close its end of the pipe and exit. Make sure that the
    // parent sees EOF.
    CheckedReadFDAtEOF(read_fd);
  }

  virtual void MultiprocessChild() override {
    int write_fd = WritePipeFD();

    char c = 'M';
    CheckedWriteFD(write_fd, &c, 1);

    pid_t pid = getpid();
    CheckedWriteFD(write_fd, &pid, sizeof(pid));

    CheckedReadFD(ReadPipeFD(), &c, 1);
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

  // Multiprocess:

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

TEST(Multiprocess, SuccessfulExit) {
  TestMultiprocessUnclean multiprocess(TestMultiprocessUnclean::kExitSuccess);
  multiprocess.Run();
}

TEST(Multiprocess, UnsuccessfulExit) {
  TestMultiprocessUnclean multiprocess(TestMultiprocessUnclean::kExitFailure);
  multiprocess.Run();
}

TEST(Multiprocess, Exit2) {
  TestMultiprocessUnclean multiprocess(TestMultiprocessUnclean::kExit2);
  multiprocess.Run();
}

TEST(Multiprocess, AbortSignal) {
  TestMultiprocessUnclean multiprocess(TestMultiprocessUnclean::kAbort);
  multiprocess.Run();
}

class TestMultiprocessClosePipe final : public Multiprocess {
 public:
  enum WhoCloses {
    kParentCloses = 0,
    kChildCloses,
  };
  enum WhatCloses {
    kReadCloses = 0,
    kWriteCloses,
    kReadAndWriteClose,
  };

  TestMultiprocessClosePipe(WhoCloses who_closes, WhatCloses what_closes)
      : Multiprocess(),
        who_closes_(who_closes),
        what_closes_(what_closes) {
  }

  ~TestMultiprocessClosePipe() {}

 private:
  void VerifyInitial() {
    ASSERT_NE(-1, ReadPipeFD());
    ASSERT_NE(-1, WritePipeFD());
  }

  // Verifies that the partner process did what it was supposed to do. This must
  // only be called when who_closes_ names the partner process, not this
  // process.
  //
  // If the partner was supposed to close its write pipe, the read pipe will be
  // checked to ensure that it shows end-of-file.
  //
  // If the partner was supposed to close its read pipe, the write pipe will be
  // checked to ensure that a checked write causes death. This can only be done
  // if the partner also provides some type of signal when it has closed its
  // read pipe, which is done in the form of it closing its write pipe, causing
  // the read pipe in this process to show end-of-file.
  void VerifyPartner() {
    if (what_closes_ == kWriteCloses) {
      CheckedReadFDAtEOF(ReadPipeFD());
    } else if (what_closes_ == kReadAndWriteClose) {
      CheckedReadFDAtEOF(ReadPipeFD());
      char c = '\0';

      // This will raise SIGPIPE. If fatal (the normal case), that will cause
      // process termination. If SIGPIPE is being handled somewhere, the write
      // will still fail and set errno to EPIPE, and CheckedWriteFD() will abort
      // execution. Regardless of how SIGPIPE is handled, the process will be
      // terminated. Because the actual termination mechanism is not known, no
      // regex can be specified.
      EXPECT_DEATH(CheckedWriteFD(WritePipeFD(), &c, 1), "");
    }
  }

  void Close() {
    switch (what_closes_) {
      case kReadCloses:
        CloseReadPipe();
        EXPECT_NE(-1, WritePipeFD());
        EXPECT_DEATH(ReadPipeFD(), "fd");
        break;
      case kWriteCloses:
        CloseWritePipe();
        EXPECT_NE(-1, ReadPipeFD());
        EXPECT_DEATH(WritePipeFD(), "fd");
        break;
      case kReadAndWriteClose:
        CloseReadPipe();
        CloseWritePipe();
        EXPECT_DEATH(ReadPipeFD(), "fd");
        EXPECT_DEATH(WritePipeFD(), "fd");
        break;
    }
  }

  // Multiprocess:

  virtual void MultiprocessParent() override {
    VerifyInitial();
    if (testing::Test::HasFatalFailure()) {
      return;
    }

    if (who_closes_ == kParentCloses) {
      Close();
    } else {
      VerifyPartner();
    }
  }

  virtual void MultiprocessChild() override {
    VerifyInitial();
    if (testing::Test::HasFatalFailure()) {
      return;
    }

    if (who_closes_ == kChildCloses) {
      Close();
    } else {
      VerifyPartner();
    }
  }

  WhoCloses who_closes_;
  WhatCloses what_closes_;

  DISALLOW_COPY_AND_ASSIGN(TestMultiprocessClosePipe);
};

TEST(MultiprocessDeathTest, ParentClosesReadPipe) {
  TestMultiprocessClosePipe multiprocess(
      TestMultiprocessClosePipe::kParentCloses,
      TestMultiprocessClosePipe::kReadCloses);
  multiprocess.Run();
}

TEST(MultiprocessDeathTest, ParentClosesWritePipe) {
  TestMultiprocessClosePipe multiprocess(
      TestMultiprocessClosePipe::kParentCloses,
      TestMultiprocessClosePipe::kWriteCloses);
  multiprocess.Run();
}

TEST(MultiprocessDeathTest, ParentClosesReadAndWritePipe) {
  TestMultiprocessClosePipe multiprocess(
      TestMultiprocessClosePipe::kParentCloses,
      TestMultiprocessClosePipe::kReadAndWriteClose);
  multiprocess.Run();
}

TEST(MultiprocessDeathTest, ChildClosesReadPipe) {
  TestMultiprocessClosePipe multiprocess(
      TestMultiprocessClosePipe::kChildCloses,
      TestMultiprocessClosePipe::kReadCloses);
  multiprocess.Run();
}

TEST(MultiprocessDeathTest, ChildClosesWritePipe) {
  TestMultiprocessClosePipe multiprocess(
      TestMultiprocessClosePipe::kChildCloses,
      TestMultiprocessClosePipe::kWriteCloses);
  multiprocess.Run();
}

TEST(MultiprocessDeathTest, ChildClosesReadAndWritePipe) {
  TestMultiprocessClosePipe multiprocess(
      TestMultiprocessClosePipe::kChildCloses,
      TestMultiprocessClosePipe::kReadAndWriteClose);
  multiprocess.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
