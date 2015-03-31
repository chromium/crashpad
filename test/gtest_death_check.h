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

#ifndef CRASHPAD_TEST_GTEST_DEATH_CHECK_H_
#define CRASHPAD_TEST_GTEST_DEATH_CHECK_H_

#include "base/logging.h"
#include "gtest/gtest.h"

//! \file

#if !(!defined(MINI_CHROMIUM_BASE_LOGGING_H_) && \
      defined(OFFICIAL_BUILD) &&                 \
      defined(NDEBUG)) ||                        \
    DOXYGEN

//! \brief Wraps the gtest `ASSERT_DEATH()` macro to make assertions about death
//!     caused by `CHECK()` failures.
//!
//! In an in-Chromium build in the official configuration in Release mode,
//! `CHECK()` does not print its condition or streamed messages. In that case,
//! this macro uses an empty \a regex pattern when calling `ASSERT_DEATH()` to
//! avoid looking for any particular output on the standard error stream. In
//! other build configurations, the \a regex pattern is left intact.
#define ASSERT_DEATH_CHECK(statement, regex) ASSERT_DEATH(statement, regex)

//! \brief Wraps the gtest `EXPECT_DEATH()` macro to make assertions about death
//!     caused by `CHECK()` failures.
//!
//! In an in-Chromium build in the official configuration in Release mode,
//! `CHECK()` does not print its condition or streamed messages. In that case,
//! this macro uses an empty \a regex pattern when calling `EXPECT_DEATH()` to
//! avoid looking for any particular output on the standard error stream. In
//! other build configurations, the \a regex pattern is left intact.
#define EXPECT_DEATH_CHECK(statement, regex) EXPECT_DEATH(statement, regex)

#else

#define ASSERT_DEATH_CHECK(statement, regex) ASSERT_DEATH(statement, "")
#define EXPECT_DEATH_CHECK(statement, regex) EXPECT_DEATH(statement, "")

#endif

#endif  // CRASHPAD_TEST_GTEST_DEATH_CHECK_H_
