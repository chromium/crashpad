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

#include "util/test/mac/mach_multiprocess.h"

#include "base/basictypes.h"
#include "gtest/gtest.h"
#include "util/file/fd_io.h"
#include "util/test/errors.h"

namespace {

using namespace crashpad;
using namespace crashpad::test;

class TestMachMultiprocess final : public MachMultiprocess {
 public:
  TestMachMultiprocess() : MachMultiprocess() {}

  ~TestMachMultiprocess() {}

 protected:
  // The base class will have already exercised the Mach ports for IPC and the
  // child task port. Just make sure that the pipe is set up correctly.
  virtual void Parent() override {
    int fd = PipeFD();

    char c;
    ssize_t rv = ReadFD(fd, &c, 1);
    ASSERT_EQ(1, rv) << ErrnoMessage("read");
    EXPECT_EQ('M', c);

    // The child will close its end of the pipe and exit. Make sure that the
    // parent sees EOF.
    rv = ReadFD(fd, &c, 1);
    ASSERT_EQ(0, rv) << ErrnoMessage("read");
  }

  virtual void Child() override {
    int fd = PipeFD();

    char c = 'M';
    ssize_t rv = WriteFD(fd, &c, 1);
    ASSERT_EQ(1, rv) << ErrnoMessage("write");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestMachMultiprocess);
};

TEST(MachMultiprocess, MachMultiprocess) {
  TestMachMultiprocess mach_multiprocess;
  mach_multiprocess.Run();
}

}  // namespace
