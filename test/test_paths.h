// Copyright 2015 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_TEST_TEST_PATHS_H_
#define CRASHPAD_TEST_TEST_PATHS_H_

#include "base/files/file_path.h"
#include "base/macros.h"

namespace crashpad {
namespace test {

//! \brief Functions to obtain paths from within tests.
class TestPaths {
 public:
  //! \brief Returns the pathname of the currently-running test executable.
  //!
  //! On failure, aborts execution.
  static base::FilePath Executable();

  //! \brief Returns the pathname of the test data root.
  //!
  //! If the `CRASHPAD_TEST_DATA_ROOT` environment variable is set, its value
  //! will be returned. Otherwise, this function will attempt to locate the test
  //! data root relative to the executable path. If this fails, it will fall
  //! back to returning the current working directory.
  //!
  //! At present, the test data root is normally the root of the Crashpad source
  //! tree, although this may not be the case indefinitely. This function may
  //! only be used to locate test data, not for arbitrary access to source
  //! files.
  static base::FilePath TestDataRoot();

  DISALLOW_IMPLICIT_CONSTRUCTORS(TestPaths);
};

}  // namespace test
}  // namespace crashpad

#endif  // CRASHPAD_TEST_TEST_PATHS_H_
