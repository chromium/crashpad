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

#include "minidump/minidump_context_writer.h"

#include <stdint.h>

#include "gtest/gtest.h"
#include "minidump/minidump_context.h"
#include "minidump/test/minidump_context_test_util.h"
#include "util/file/string_file_writer.h"

namespace crashpad {
namespace test {
namespace {

TEST(MinidumpContextWriter, MinidumpContextX86Writer) {
  StringFileWriter file_writer;

  {
    // Make sure that a context writer that’s untouched writes a zeroed-out
    // context.
    SCOPED_TRACE("zero");

    MinidumpContextX86Writer context_writer;

    EXPECT_TRUE(context_writer.WriteEverything(&file_writer));
    ASSERT_EQ(sizeof(MinidumpContextX86), file_writer.string().size());

    const MinidumpContextX86* observed =
        reinterpret_cast<const MinidumpContextX86*>(&file_writer.string()[0]);
    ExpectMinidumpContextX86(0, observed);
  }

  {
    SCOPED_TRACE("nonzero");

    file_writer.Reset();
    const uint32_t kSeed = 0x8086;

    MinidumpContextX86Writer context_writer;
    InitializeMinidumpContextX86(context_writer.context(), kSeed);

    EXPECT_TRUE(context_writer.WriteEverything(&file_writer));
    ASSERT_EQ(sizeof(MinidumpContextX86), file_writer.string().size());

    const MinidumpContextX86* observed =
        reinterpret_cast<const MinidumpContextX86*>(&file_writer.string()[0]);
    ExpectMinidumpContextX86(kSeed, observed);
  }
}

TEST(MinidumpContextWriter, MinidumpContextAMD64Writer) {
  StringFileWriter file_writer;

  {
    // Make sure that a context writer that’s untouched writes a zeroed-out
    // context.
    SCOPED_TRACE("zero");

    MinidumpContextAMD64Writer context_writer;

    EXPECT_TRUE(context_writer.WriteEverything(&file_writer));
    ASSERT_EQ(sizeof(MinidumpContextAMD64), file_writer.string().size());

    const MinidumpContextAMD64* observed =
        reinterpret_cast<const MinidumpContextAMD64*>(&file_writer.string()[0]);
    ExpectMinidumpContextAMD64(0, observed);
  }

  {
    SCOPED_TRACE("nonzero");

    file_writer.Reset();
    const uint32_t kSeed = 0x808664;

    MinidumpContextAMD64Writer context_writer;
    InitializeMinidumpContextAMD64(context_writer.context(), kSeed);

    EXPECT_TRUE(context_writer.WriteEverything(&file_writer));
    ASSERT_EQ(sizeof(MinidumpContextAMD64), file_writer.string().size());

    const MinidumpContextAMD64* observed =
        reinterpret_cast<const MinidumpContextAMD64*>(&file_writer.string()[0]);
    ExpectMinidumpContextAMD64(kSeed, observed);
  }
}

}  // namespace
}  // namespace test
}  // namespace crashpad
