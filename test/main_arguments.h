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

#ifndef CRASHPAD_TEST_MAIN_ARGUMENTS_H_
#define CRASHPAD_TEST_MAIN_ARGUMENTS_H_

namespace crashpad {
namespace test {

//! \brief Saves pointers to the arguments to `main()` for later use.
//!
//! Call this function from a test program’s `main()` function so that tests
//! that require access to these variables can retrieve them from
//! GetMainArguments().
//!
//! This function must be called exactly once during the lifetime of a test
//! program.
void InitializeMainArguments(const int *argc, char* *const *argv);

//! \brief Retrieves pointers to the arguments to `main()`.
//!
//! Tests that need to access the original values of a test program’s `main()`
//! function can use this function to retrieve them, provided that `main()`
//! called InitializeMainArguments().
void GetMainArguments(const int **argc, const char *const *const **argv);

}  // namespace test
}  // namespace crashpad

#endif  // CRASHPAD_TEST_MAIN_ARGUMENTS_H_
