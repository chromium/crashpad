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

#ifndef CRASHPAD_UTIL_TEST_MULTIPROCESS_H_
#define CRASHPAD_UTIL_TEST_MULTIPROCESS_H_

#include <unistd.h>

#include "base/basictypes.h"

namespace crashpad {
namespace test {

namespace internal {
struct MultiprocessInfo;
};

//! \brief Manages a multiprocess test.
//!
//! These tests are `fork()`-based. The parent and child processes are able to
//! communicate via a pair of POSIX pipes.
//!
//! Subclasses are expected to implement the parent and child by overriding the
//! appropriate methods.
class Multiprocess {
 public:
  Multiprocess();

  //! \brief Runs the test.
  //!
  //! This method establishes the proper testing environment by calling
  //! PreFork(), then calls `fork()`. In the parent process, it calls
  //! RunParent(), and in the child process, it calls RunChild().
  //!
  //! This method uses gtest assertions to validate the testing environment. If
  //! the testing environment cannot be set up properly, it is possible that
  //! MultiprocessParent() or MultiprocessChild() will not be called. In the
  //! parent process, this method also waits for the child process to exit after
  //! MultiprocessParent() returns, and verifies that it exited cleanly without
  //! raising any gtest assertions.
  void Run();

 protected:
  ~Multiprocess();

  //! \brief Establishes the proper testing environment prior to forking.
  //!
  //! Subclasses that solely implement a test should not need to override this
  //! method. Subclasses that do not implement tests but instead implement
  //! additional testing features on top of this class may override this method
  //! provided that they call the superclass’ implementation first as follows:
  //!
  //! \code
  //!   virtual void PreFork() override {
  //!     Multiprocess::PreFork();
  //!     if (testing::Test::HasFatalFailure()) {
  //!       return;
  //!     }
  //!
  //!     // Place subclass-specific pre-fork code here.
  //!   }
  //! \endcode
  //!
  //! Subclass implementations may signal failure by raising their own fatal
  //! gtest assertions.
  virtual void PreFork();

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

 private:
  //! \brief Runs the parent side of the test.
  //!
  //! This method establishes the parent’s environment and calls
  //! MultiprocessParent().
  void RunParent();

  //! \brief Runs the child side of the test.
  //!
  //! This method establishes the child’s environment, calls
  //! MultiprocessChild(), and exits cleanly. However, if any failure (via fatal
  //! or nonfatal gtest assertion) is detected, the child will exit with a
  //! failure status.
  void RunChild();

  //! \brief The subclass-provided parent routine.
  //!
  //! Test failures should be reported via gtest: `EXPECT_*()`, `ASSERT_*()`,
  //! `FAIL()`, etc.
  //!
  //! This method must not use a `wait()`-family system call to wait for the
  //! child process to exit, as this is handled by this class.
  //!
  //! Subclasses must implement this method to define how the parent operates.
  virtual void MultiprocessParent() = 0;

  //! \brief The subclass-provided child routine.
  //!
  //! Test failures should be reported via gtest: `EXPECT_*()`, `ASSERT_*()`,
  //! `FAIL()`, etc.
  //!
  //! Subclasses must implement this method to define how the child operates.
  virtual void MultiprocessChild() = 0;

  internal::MultiprocessInfo* info_;

  DISALLOW_COPY_AND_ASSIGN(Multiprocess);
};

}  // namespace test
}  // namespace crashpad

#endif  // CRASHPAD_UTIL_TEST_MULTIPROCESS_H_
