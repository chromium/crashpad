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

#ifndef CRASHPAD_UTIL_TEST_MAC_MACH_MULTIPROCESS_H_
#define CRASHPAD_UTIL_TEST_MAC_MACH_MULTIPROCESS_H_

#include <mach/mach.h>
#include <unistd.h>

#include "base/basictypes.h"

namespace crashpad {
namespace test {

namespace internal {
struct MachMultiprocessInfo;
}  // namespace internal

//! \brief Manages a Mach-aware multiprocess test.
//!
//! These tests are `fork()`-based. The parent process has access to the child
//! process’ task port. The parent and child processes are able to communicate
//! via Mach IPC, and via a pair of POSIX pipes.
//!
//! Subclasses are expected to implement the parent and child by overriding the
//! appropriate methods.
class MachMultiprocess {
 public:
  MachMultiprocess();

  //! \brief Runs the test.
  //!
  //! This method establishes the proper testing environment and calls
  //! RunParent() in the parent process and RunChild() in the child process.
  //!
  //! This method uses gtest assertions to validate the testing environment. If
  //! the testing environment cannot be set up properly, it is possible that
  //! Parent() or Child() will not be called. In the parent process, this method
  //! also waits for the child process to exit after Parent() returns, and
  //! verifies that it exited cleanly with gtest assertions.
  void Run();

 protected:
  ~MachMultiprocess();

  //! \brief The subclass-provided parent routine.
  //!
  //! Test failures should be reported via gtest: `EXPECT_*()`, `ASSERT_*()`,
  //! `FAIL()`, etc.
  //!
  //! This method must not use a `wait()`-family system call to wait for the
  //! child process to exit, as this is handled by RunParent().
  //!
  //! Subclasses must implement this method to define how the parent operates.
  virtual void Parent() = 0;

  //! \brief The subclass-provided child routine.
  //!
  //! Test failures should be reported via gtest: `EXPECT_*()`, `ASSERT_*()`,
  //! `FAIL()`, etc.
  //!
  //! Subclasses must implement this method to define how the child operates.
  virtual void Child() = 0;

  //! \brief Returns the child process’ process ID.
  //!
  //! This method may only be called by the parent process.
  pid_t ChildPID() const;

  //! \brief Returns the read pipe’s file descriptor.
  //!
  //! This method may be called by either the parent or the child process.
  //! Anything written to the write pipe in the partner process will appear
  //! on the this file descriptor in this process.
  int ReadPipeFD() const;

  //! \brief Returns the write pipe’s file descriptor.
  //!
  //! This method may be called by either the parent or the child process.
  //! Anything written to this file descriptor in this process will appear on
  //! the read pipe in the partner process.
  int WritePipeFD() const;

  //! \brief Returns a receive right for the local port.
  //!
  //! This method may be called by either the parent or the child process. It
  //! returns a receive right, with a corresponding send right held in the
  //! opposing process.
  mach_port_t LocalPort() const;

  //! \brief Returns a send right for the remote port.
  //!
  //! This method may be called by either the parent or the child process. It
  //! returns a send right, with the corresponding receive right held in the
  //! opposing process.
  mach_port_t RemotePort() const;

  //! \brief Returns a send right for the child’s task port.
  //!
  //! This method may only be called by the parent process.
  mach_port_t ChildTask() const;

 private:
  //! \brief Runs the parent side of the test.
  //!
  //! This method establishes the parent’s environment, performs the handshake
  //! with the child, calls Parent(), and waits for the child process to exit.
  //! Assuming that the environment can be set up correctly and the child exits
  //! successfully, the test will pass.
  void RunParent();

  //! \brief Runs the child side of the test.
  //!
  //! This method establishes the child’s environment, performs the handshake
  //! with the parent, calls Child(), and exits cleanly. However, if any failure
  //! (via fatal or nonfatal gtest assertion) is detected, the child will exit
  //! with a failure status.
  void RunChild();

  internal::MachMultiprocessInfo* info_;

  DISALLOW_COPY_AND_ASSIGN(MachMultiprocess);
};

}  // namespace test
}  // namespace crashpad

#endif  // CRASHPAD_UTIL_TEST_MAC_MACH_MULTIPROCESS_H_
