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

#include "snapshot/process_types/crashpad_info_reader.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "client/crashpad_info.h"
#include "gtest/gtest.h"
#include "test/multiprocess.h"
#include "util/file/file_io.h"
#include "util/misc/address_types.h"
#include "util/misc/from_pointer_cast.h"
#include "util/process/process_memory_linux.h"
#include "util/process/process_memory_range.h"

namespace crashpad {
namespace test {
namespace {

class ReadFromChildTest : public Multiprocess {
 public:
  ReadFromChildTest() : Multiprocess() {
    CrashpadInfo* info = CrashpadInfo::GetCrashpadInfo();
    info->set_extra_memory_ranges(nullptr);
    info->set_simple_annotations(nullptr);
    info->set_annotations_list(nullptr);
    info->set_crashpad_handler_behavior(kCrashpadHandlerBehavior);
    info->set_system_crash_reporter_forwarding(kSystemCrashReporterForwarding);
    info->set_gather_indirectly_referenced_memory(
        kGatherIndirectlyReferencedMemory, 0);
    info->AddUserDataMinidumpStream(42, this, sizeof(*this));
  }

  ~ReadFromChildTest() {}

 private:
  void MultiprocessParent() {
    ProcessMemoryLinux memory;
    ASSERT_TRUE(memory.Initialize(ChildPID()));

#if defined(ARCH_CPU_64_BITS)
    constexpr bool am_64_bit = true;
#else
    constexpr bool am_64_bit = false;
#endif

    ProcessMemoryRange range;
    ASSERT_TRUE(range.Initialize(&memory, am_64_bit));

    CrashpadInfo* info = CrashpadInfo::GetCrashpadInfo();

    CrashpadInfoReader reader;
    ASSERT_TRUE(reader.Initialize(&range, FromPointerCast<VMAddress>(info)));
    EXPECT_EQ(reader.CrashpadHandlerBehavior(),
              TriState{kCrashpadHandlerBehavior});
    EXPECT_EQ(reader.SystemCrashReporterForwarding(),
              TriState{kSystemCrashReporterForwarding});
    EXPECT_EQ(reader.GatherIndirectlyReferencedMemory(),
              TriState{kGatherIndirectlyReferencedMemory});
    EXPECT_EQ(reader.ExtraMemoryRanges(), FromPointerCast<VMAddress>(nullptr));
    EXPECT_EQ(reader.SimpleAnnotations(),
              FromPointerCast<VMAddress>(info->simple_annotations()));
    EXPECT_EQ(reader.AnnotationsList(),
              FromPointerCast<VMAddress>(info->annotations_list()));
    // TODO(jperaza): Read UserDataMinidumpStreams.
  }

  void MultiprocessChild() { CheckedReadFileAtEOF(ReadPipeHandle()); }

  static constexpr TriState kCrashpadHandlerBehavior = TriState::kEnabled;
  static constexpr TriState kSystemCrashReporterForwarding =
      TriState::kDisabled;
  static constexpr TriState kGatherIndirectlyReferencedMemory =
      TriState::kUnset;

  DISALLOW_COPY_AND_ASSIGN(ReadFromChildTest);
};

TEST(CrashpadInfoReader, ReadFromChild) {
  ReadFromChildTest test;
  test.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
