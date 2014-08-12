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

#ifndef CRASHPAD_MINIDUMP_MINIDUMP_TEST_UTIL_H_
#define CRASHPAD_MINIDUMP_MINIDUMP_TEST_UTIL_H_

#include <dbghelp.h>
#include <stdint.h>

namespace crashpad {
namespace test {

//! \brief Verifies, via gtest assertions, that a MINIDUMP_HEADER contains
//!     expected values.
//!
//! All fields in the MINIDUMP_HEADER will be evaluated. Most are compared to
//! their correct default values. MINIDUMP_HEADER::NumberOfStreams is compared
//! to \a streams, and MINIDUMP_HEADER::TimeDateStamp is compared to \a
//! timestamp. Most fields are checked with nonfatal EXPECT-style assertions,
//! but MINIDUMP_HEADER::NumberOfStreams and MINIDUMP_HEADER::StreamDirectoryRva
//! are checked with fatal ASSERT-style assertions, because they must be
//! correct in order for processing of the minidump to continue.
void VerifyMinidumpHeader(const MINIDUMP_HEADER* header,
                          uint32_t streams,
                          uint32_t timestamp);

}  // namespace test
}  // namespace crashpad

#endif  // CRASHPAD_MINIDUMP_MINIDUMP_TEST_UTIL_H_
