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

#include "util/stdlib/strlcpy.h"

#include <string.h>

#include <algorithm>

#include "base/format_macros.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "gtest/gtest.h"

namespace crashpad {
namespace test {
namespace {

TEST(strlcpy, c16lcpy) {
  // Use a destination buffer that’s larger than the length passed to c16lcpy.
  // The unused portion is a guard area that must not be written to.
  struct TestBuffer {
    base::char16 lead_guard[64];
    base::char16 data[128];
    base::char16 trail_guard[64];
  };
  TestBuffer expected_untouched;
  memset(&expected_untouched, 0xa5, sizeof(expected_untouched));

  // Test with M, é, Ā, ő, and Ḙ. This is a mix of characters that have zero and
  // nonzero low and high bytes.
  const base::char16 test_characters[] = {0x4d, 0xe9, 0x100, 0x151, 0x1e18};

  for (size_t index = 0; index < arraysize(test_characters); ++index) {
    base::char16 test_character = test_characters[index];
    SCOPED_TRACE(base::StringPrintf(
        "character index %" PRIuS ", character 0x%x", index, test_character));
    for (size_t length = 0; length < 256; ++length) {
      SCOPED_TRACE(
          base::StringPrintf("index %" PRIuS, length));
      base::string16 test_string(length, test_character);

      TestBuffer destination;
      memset(&destination, 0xa5, sizeof(destination));

      EXPECT_EQ(length,
                c16lcpy(destination.data,
                        test_string.c_str(),
                        arraysize(destination.data)));

      // Make sure that the destination buffer is NUL-terminated, and that as
      // much of the test string was copied as could fit.
      size_t expected_destination_length =
          std::min(length, arraysize(destination.data) - 1);

      EXPECT_EQ('\0', destination.data[expected_destination_length]);
      EXPECT_EQ(expected_destination_length, base::c16len(destination.data));
      EXPECT_TRUE(base::c16memcmp(test_string.c_str(),
                                  destination.data,
                                  expected_destination_length) == 0);

      // Make sure that the portion of the destination buffer that was not used
      // was not touched. This includes the guard areas and the unused portion
      // of the buffer passed to c16lcpy.
      EXPECT_TRUE(base::c16memcmp(expected_untouched.lead_guard,
                                  destination.lead_guard,
                                  arraysize(destination.lead_guard)) == 0);
      size_t expected_untouched_length =
          arraysize(destination.data) - expected_destination_length - 1;
      EXPECT_TRUE(
          base::c16memcmp(expected_untouched.data,
                          &destination.data[expected_destination_length + 1],
                          expected_untouched_length) == 0);
      EXPECT_TRUE(base::c16memcmp(expected_untouched.trail_guard,
                                  destination.trail_guard,
                                  arraysize(destination.trail_guard)) == 0);
    }
  }
}

}  // namespace
}  // namespace test
}  // namespace crashpad
