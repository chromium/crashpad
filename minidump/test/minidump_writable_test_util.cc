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

#include "minidump/test/minidump_writable_test_util.h"

#include <string>

#include "gtest/gtest.h"

namespace crashpad {
namespace test {

const void* MinidumpWritableAtRVAInternal(const std::string& file_contents,
                                          RVA rva) {
  if (rva >= file_contents.size()) {
    EXPECT_LT(rva, file_contents.size());
    return nullptr;
  }

  return &file_contents[rva];
}

const void* MinidumpWritableAtLocationDescriptorInternal(
    const std::string& file_contents,
    const MINIDUMP_LOCATION_DESCRIPTOR& location,
    size_t expected_minimum_size) {
  if (location.DataSize == 0) {
    EXPECT_EQ(0u, location.Rva);
    return nullptr;
  } else if (location.DataSize < expected_minimum_size) {
    EXPECT_GE(location.DataSize, expected_minimum_size);
    return nullptr;
  }

  RVA end = location.Rva + location.DataSize;
  if (end > file_contents.size()) {
    EXPECT_LE(end, file_contents.size());
    return nullptr;
  }

  const void* rv = MinidumpWritableAtRVAInternal(file_contents, location.Rva);

  return rv;
}

}  // namespace test
}  // namespace crashpad
