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

#include "minidump/test/minidump_file_writer_test_util.h"

#include "gtest/gtest.h"

namespace crashpad {
namespace test {

void VerifyMinidumpHeader(const MINIDUMP_HEADER* header,
                          uint32_t streams,
                          uint32_t timestamp) {
  EXPECT_EQ(static_cast<uint32_t>(MINIDUMP_SIGNATURE), header->Signature);
  EXPECT_EQ(static_cast<uint32_t>(MINIDUMP_VERSION), header->Version);
  ASSERT_EQ(streams, header->NumberOfStreams);
  ASSERT_EQ(streams ? sizeof(MINIDUMP_HEADER) : 0u, header->StreamDirectoryRva);
  EXPECT_EQ(0u, header->CheckSum);
  EXPECT_EQ(timestamp, header->TimeDateStamp);
  EXPECT_EQ(MiniDumpNormal, header->Flags);
}

}  // namespace test
}  // namespace crashpad
