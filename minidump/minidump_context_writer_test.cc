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
#include "minidump/test/minidump_writable_test_util.h"
#include "snapshot/cpu_context.h"
#include "snapshot/test/test_cpu_context.h"
#include "util/file/string_file.h"

namespace crashpad {
namespace test {
namespace {

TEST(MinidumpContextWriter, MinidumpContextX86Writer) {
  StringFile string_file;

  {
    // Make sure that a context writer that’s untouched writes a zeroed-out
    // context.
    SCOPED_TRACE("zero");

    MinidumpContextX86Writer context_writer;

    EXPECT_TRUE(context_writer.WriteEverything(&string_file));
    ASSERT_EQ(sizeof(MinidumpContextX86), string_file.string().size());

    const MinidumpContextX86* observed =
        MinidumpWritableAtRVA<MinidumpContextX86>(string_file.string(), 0);
    ASSERT_TRUE(observed);

    ExpectMinidumpContextX86(0, observed, false);
  }

  {
    SCOPED_TRACE("nonzero");

    string_file.Reset();
    const uint32_t kSeed = 0x8086;

    MinidumpContextX86Writer context_writer;
    InitializeMinidumpContextX86(context_writer.context(), kSeed);

    EXPECT_TRUE(context_writer.WriteEverything(&string_file));
    ASSERT_EQ(sizeof(MinidumpContextX86), string_file.string().size());

    const MinidumpContextX86* observed =
        MinidumpWritableAtRVA<MinidumpContextX86>(string_file.string(), 0);
    ASSERT_TRUE(observed);

    ExpectMinidumpContextX86(kSeed, observed, false);
  }
}

TEST(MinidumpContextWriter, MinidumpContextAMD64Writer) {
  StringFile string_file;

  {
    // Make sure that a context writer that’s untouched writes a zeroed-out
    // context.
    SCOPED_TRACE("zero");

    MinidumpContextAMD64Writer context_writer;

    EXPECT_TRUE(context_writer.WriteEverything(&string_file));
    ASSERT_EQ(sizeof(MinidumpContextAMD64), string_file.string().size());

    const MinidumpContextAMD64* observed =
        MinidumpWritableAtRVA<MinidumpContextAMD64>(string_file.string(), 0);
    ASSERT_TRUE(observed);

    ExpectMinidumpContextAMD64(0, observed, false);
  }

  {
    SCOPED_TRACE("nonzero");

    string_file.Reset();
    const uint32_t kSeed = 0x808664;

    MinidumpContextAMD64Writer context_writer;
    InitializeMinidumpContextAMD64(context_writer.context(), kSeed);

    EXPECT_TRUE(context_writer.WriteEverything(&string_file));
    ASSERT_EQ(sizeof(MinidumpContextAMD64), string_file.string().size());

    const MinidumpContextAMD64* observed =
        MinidumpWritableAtRVA<MinidumpContextAMD64>(string_file.string(), 0);
    ASSERT_TRUE(observed);

    ExpectMinidumpContextAMD64(kSeed, observed, false);
  }
}

TEST(MinidumpContextWriter, CreateFromSnapshot_X86) {
  const uint32_t kSeed = 32;

  CPUContextX86 context_snapshot_x86;
  CPUContext context_snapshot;
  context_snapshot.x86 = &context_snapshot_x86;
  InitializeCPUContextX86(&context_snapshot, kSeed);

  scoped_ptr<MinidumpContextWriter> context_writer =
      MinidumpContextWriter::CreateFromSnapshot(&context_snapshot);
  ASSERT_TRUE(context_writer);

  StringFile string_file;
  ASSERT_TRUE(context_writer->WriteEverything(&string_file));

  const MinidumpContextX86* observed =
      MinidumpWritableAtRVA<MinidumpContextX86>(string_file.string(), 0);
  ASSERT_TRUE(observed);

  ExpectMinidumpContextX86(kSeed, observed, true);
}

TEST(MinidumpContextWriter, CreateFromSnapshot_AMD64) {
  const uint32_t kSeed = 64;

  CPUContextX86_64 context_snapshot_x86_64;
  CPUContext context_snapshot;
  context_snapshot.x86_64 = &context_snapshot_x86_64;
  InitializeCPUContextX86_64(&context_snapshot, kSeed);

  scoped_ptr<MinidumpContextWriter> context_writer =
      MinidumpContextWriter::CreateFromSnapshot(&context_snapshot);
  ASSERT_TRUE(context_writer);

  StringFile string_file;
  ASSERT_TRUE(context_writer->WriteEverything(&string_file));

  const MinidumpContextAMD64* observed =
      MinidumpWritableAtRVA<MinidumpContextAMD64>(string_file.string(), 0);
  ASSERT_TRUE(observed);

  ExpectMinidumpContextAMD64(kSeed, observed, true);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
