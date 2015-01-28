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

#ifndef CRASHPAD_UTIL_TEST_MULTIPROCESS_EXEC_H_
#define CRASHPAD_UTIL_TEST_MULTIPROCESS_EXEC_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "build/build_config.h"
#include "util/test/multiprocess.h"

namespace crashpad {
namespace test {

//! \brief Manages an `exec()`-based multiprocess test.
//!
//! These tests are based on `fork()` and `exec()`. The parent process is able
//! to communicate with the child in the same manner as a base-class
//! Multiprocess parent. The read and write pipes appear in the child process on
//! stdin and stdout, respectively.
//!
//! Subclasses are expected to implement the parent in the same was as a
//! base-class Multiprocess parent. The child must be implemented in an
//! executable to be set by SetChildCommand().
class MultiprocessExec : public Multiprocess {
 public:
  MultiprocessExec();

  //! \brief Sets the command to `exec()` in the child.
  //!
  //! This method must be called before the test can be Run().
  //!
  //! \param[in] command The executable’s pathname.
  //! \param[in] arguments The command-line arguments to pass to the child
  //!     process in its `argv[]` vector. This vector must begin at `argv[1]`,
  //!     as \a command is implicitly used as `argv[0]`. This argument may be
  //!     `nullptr` if no command-line arguments are to be passed.
  void SetChildCommand(const std::string& command,
                       const std::vector<std::string>* arguments);

 protected:
  ~MultiprocessExec();

  // Multiprocess:
  void PreFork() override;

 private:
  // Multiprocess:
  void MultiprocessChild() override;

  std::string command_;
  std::vector<std::string> arguments_;
#if defined(OS_POSIX)
  std::vector<const char*> argv_;
#elif defined(OS_WIN)
  std::wstring command_line_;
#endif  // OS_POSIX

  DISALLOW_COPY_AND_ASSIGN(MultiprocessExec);
};

}  // namespace test
}  // namespace crashpad

#endif  // CRASHPAD_UTIL_TEST_MULTIPROCESS_EXEC_H_
