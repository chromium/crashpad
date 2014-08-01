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

#include "util/misc/uuid.h"

#include <stdint.h>

#include <string>

#include "base/basictypes.h"
#include "gtest/gtest.h"

namespace {

using namespace crashpad;

TEST(UUID, UUID) {
  UUID uuid_zero;
  for (size_t index = 0; index < 16; ++index) {
    EXPECT_EQ(0u, uuid_zero.data[index]);
  }
  EXPECT_EQ("00000000-0000-0000-0000-000000000000", uuid_zero.ToString());

  const uint8_t kBytes[16] = {0x00,
                              0x01,
                              0x02,
                              0x03,
                              0x04,
                              0x05,
                              0x06,
                              0x07,
                              0x08,
                              0x09,
                              0x0a,
                              0x0b,
                              0x0c,
                              0x0d,
                              0x0e,
                              0x0f};
  UUID uuid(kBytes);
  for (size_t index = 0; index < arraysize(kBytes); ++index) {
    EXPECT_EQ(kBytes[index], uuid.data[index]);
  }
  EXPECT_EQ("00010203-0405-0607-0809-0a0b0c0d0e0f", uuid.ToString());

  // UUID is a standard-layout structure. It is valid to memcpy to it.
  const uint8_t kMoreBytes[16] = {0xff,
                                  0xee,
                                  0xdd,
                                  0xcc,
                                  0xbb,
                                  0xaa,
                                  0x99,
                                  0x88,
                                  0x77,
                                  0x66,
                                  0x55,
                                  0x44,
                                  0x33,
                                  0x22,
                                  0x11,
                                  0x00};
  memcpy(&uuid, kMoreBytes, sizeof(kMoreBytes));
  EXPECT_EQ("ffeeddcc-bbaa-9988-7766-554433221100", uuid.ToString());
}

}  // namespace
