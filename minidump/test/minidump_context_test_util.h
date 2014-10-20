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

#ifndef CRASHPAD_MINIDUMP_TEST_MINIDUMP_CONTEXT_TEST_UTIL_H_
#define CRASHPAD_MINIDUMP_TEST_MINIDUMP_CONTEXT_TEST_UTIL_H_

#include <stdint.h>

#include "minidump/minidump_context.h"

namespace crashpad {
namespace test {

//! \brief Initializes a context structure for testing.
//!
//! \param[out] context The structure to initialize.
//! \param[in] seed The seed value. Initializing two context structures of the
//!     same type with identical seed values should produce identical context
//!     structures. Initialization with a different seed value should produce
//!     a different context structure. If \a seed is `0`, \a context is zeroed
//!     out entirely except for the flags field, which will identify the context
//!     type. If \a seed is nonzero \a context will be populated entirely with
//!     nonzero values.
//!
//! \{
void InitializeMinidumpContextX86(MinidumpContextX86* context, uint32_t seed);
void InitializeMinidumpContextAMD64(MinidumpContextAMD64* context,
                                    uint32_t seed);
//! \}

//! \brief Verifies, via gtest assertions, that a context structure contains
//!     expected values.
//!
//! \param[in] expect_seed The seed value used to initialize a context
//!     structure. This is the seed value used with
//!     InitializeMinidumpContext*().
//! \param[in] observed The context structure to check. All fields of this
//!     structure will be compared against the expected context structure, one
//!     initialized with \a expect_seed.
//! \{
void ExpectMinidumpContextX86(uint32_t expect_seed,
                              const MinidumpContextX86* observed);
void ExpectMinidumpContextAMD64(uint32_t expect_seed,
                                const MinidumpContextAMD64* observed);
//! \}

}  // namespace test
}  // namespace crashpad

#endif  // CRASHPAD_MINIDUMP_TEST_MINIDUMP_CONTEXT_TEST_UTIL_H_
