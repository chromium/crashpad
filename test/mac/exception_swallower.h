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

#ifndef CRASHPAD_TEST_MAC_EXCEPTION_SWALLOWER_H_
#define CRASHPAD_TEST_MAC_EXCEPTION_SWALLOWER_H_

#include <sys/types.h>

#include <string>

#include "base/files/scoped_file.h"
#include "base/macros.h"

namespace crashpad {
namespace test {

//! \brief Swallows `EXC_CRASH` and `EXC_CORPSE_NOTIFY` exceptions in test child
//!     processes.
//!
//! This class is intended to be used by test code that crashes intentionally.
//!
//! On macOS, the system’s crash reporter normally saves crash reports for all
//! crashes in test code, by virtue of being set as the `EXC_CRASH` or
//! `EXC_CORPSE_NOTIFY` handler. This litters the user’s
//! `~/Library/Logs/DiagnosticReports` directory and can be time-consuming.
//! Reports generated for code that crashes intentionally have no value, and
//! many Crashpad tests do crash intentionally.
//!
//! In order to prevent the system’s crash reporter from handling intentional
//! crashes, use this class. First, ensure that the exception swallower server
//! process, `crashpad_exception_swallower`, is running by calling
//! Parent_PrepareForCrashingChild() or Parent_PrepareForGtestDeathTest() from
//! the parent test process. Then, call Child_SwallowExceptions() from the test
//! child process that crashes intentionally.
//!
//! Don’t call Child_SwallowExceptions() except in test child processes that are
//! expected to crash. It is invalid to call Child_SwallowExceptions() in the
//! parent test process.
//!
//! An exception swallower server process started by this interface will live as
//! long as the process that created it, and will then exit.
//!
//! Crashpad’s ASSERT_DEATH_CRASH(), EXPECT_DEATH_CRASH(), ASSERT_DEATH_CHECK(),
//! and EXPECT_DEATH_CHECK() macros make use of this class on macOS, as does the
//! Multiprocess test interface.
class ExceptionSwallower {
 public:
  //! \brief In a parent test process, prepares for a crashing child process
  //!     whose exceptions are to be swallowed.
  //!
  //! Calling this in a parent test process starts an exception swallower server
  //! process if none has been started yet. Subsequently, a forked child process
  //! expecting to crash can call Child_SwallowExceptions() to direct exceptions
  //! to the exception swallower server process. Multiple children can share a
  //! single exception swallower server process.
  //!
  //! This function establishes the exception swallower server unconditionally.
  //! This is not appropriate for gtest death tests, which should use
  //! Parent_PrepareForGtestDeathTest() instead.
  static void Parent_PrepareForCrashingChild();

  //! \brief In a parent test process, prepares for a gtest death test whose
  //!     exceptions are to be swallowed.
  //!
  //! This is similar to Parent_PrepareForCrashingChild(), except it only starts
  //! an exception swallower server process if the <a
  //! href="https://github.com/google/googletest/blob/master/googletest/docs/AdvancedGuide.md#death-test-styles">gtest
  //! death test style</a> is “fast”. With the “fast” style, the death test is
  //! run directly from a forked child. The alternative, “threadsafe”,
  //! reexecutes the test executable to run the death test. Since the death test
  //! does not run directly forked from the parent test process, the parent’s
  //! ExceptionSwallower object would not be available to the child, rendering
  //! any exception swallower server process started by a parent test process
  //! unavailable to the child. Since such an exception swallower server process
  //! would go unused, this function will not start one when running under the
  //! “threadsafe” style. In that case, each child death test is responsible for
  //! starting its own exception swallower server process, and this will occur
  //! when child death tests call Child_SwallowExceptions().
  //!
  //! This function establishes the exception swallower server conditionally
  //! based on the gtest death test style. This is not appropriate for tests
  //! that unconditionally fork a child that intentionally crashes without an
  //! intervening execv(). For such tests, use Parent_PrepareForCrashingChild()
  //! instead.
  static void Parent_PrepareForGtestDeathTest();

  //! \brief In a test child process, arranges to swallow `EXC_CRASH` and
  //!     `EXC_CORPSE_NOTIFY` exceptions.
  //!
  //! This must be called in a test child process. It must not be called from a
  //! parent test process directly.
  //!
  //! It is not an error to call this in a child process without having first
  //! called Parent_PrepareForCrashingChild() or
  //! Parent_PrepareForGtestDeathTest() in the parent process, but failing to do
  //! so precludes the possibility of multiple qualified child processes sharing
  //! a single exception swallower server process. In this context, children
  //! running directly from a forked parent are qualified. gtest death tests
  //! under the “threadsafe” <a
  //! href="https://github.com/google/googletest/blob/master/googletest/docs/AdvancedGuide.md#death-test-styles">gtest
  //! death test style</a> are not qualified.
  static void Child_SwallowExceptions();

 private:
  ExceptionSwallower();
  ~ExceptionSwallower();

  //! \brief Returns the ExceptionSwallower singleton.
  //!
  //! If the object does not yet exist, it will be created, and the exception
  //! swallower server process, `crashpad_exception_swallower`, will be started.
  static ExceptionSwallower* Get();

  //! \brief Identifies the calling process as the test parent.
  //!
  //! This is used to check for interface abuses. Its use is optional, but if
  //! it’s called, SwallowExceptions() must not be called from the same process.
  void SetParent();

  //! \brief In a test child process, arranges to swallow `EXC_CRASH` and
  //!     `EXC_CORPSE_NOTIFY` exceptions.
  //!
  //! This must be called in a test child process. It must not be called from a
  //! parent test process directly.
  void SwallowExceptions();

  std::string service_name_;

  // fd_ is half of a socketpair() that serves a dual purpose. The exception
  // swallower server process writes its service name to the socket once
  // registered, allowing the parent test process to obtain a reference to the
  // service. The socket remains open in the parent test process so that the
  // exception swallower server process can observe, based on reading
  // end-of-file, when the parent test process has died. The exception swallower
  // server process uses this as a signal that it’s safe to exit.
  base::ScopedFD fd_;

  pid_t parent_pid_;

  DISALLOW_COPY_AND_ASSIGN(ExceptionSwallower);
};

}  // namespace test
}  // namespace crashpad

#endif  // CRASHPAD_TEST_MAC_EXCEPTION_SWALLOWER_H_
