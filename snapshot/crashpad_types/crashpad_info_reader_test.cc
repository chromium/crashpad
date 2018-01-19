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

#include "snapshot/crashpad_types/crashpad_info_reader.h"

#include <sys/types.h>
#include <unistd.h>

#include "build/build_config.h"
#include "client/annotation_list.h"
#include "client/crashpad_info.h"
#include "client/simple_address_range_bag.h"
#include "client/simple_string_dictionary.h"
#include "gtest/gtest.h"
#include "test/multiprocess.h"
#include "test/process_type.h"
#include "util/file/file_io.h"
#include "util/misc/from_pointer_cast.h"
#include "util/process/process_memory_native.h"

#if defined(OS_FUCHSIA)
#include <zircon/process.h>
#endif

namespace crashpad {
namespace test {
namespace {

constexpr TriState kCrashpadHandlerBehavior = TriState::kEnabled;
constexpr TriState kSystemCrashReporterForwarding = TriState::kDisabled;
constexpr TriState kGatherIndirectlyReferencedMemory = TriState::kUnset;

constexpr uint32_t kIndirectlyReferencedMemoryCap = 42;

class CrashpadInfoTest {
 public:
  CrashpadInfoTest()
      : extra_memory_(), simple_annotations_(), annotation_list_() {
    CrashpadInfo* info = CrashpadInfo::GetCrashpadInfo();
    info->set_extra_memory_ranges(&extra_memory_);
    info->set_simple_annotations(&simple_annotations_);
    info->set_annotations_list(&annotation_list_);
    info->set_crashpad_handler_behavior(kCrashpadHandlerBehavior);
    info->set_system_crash_reporter_forwarding(kSystemCrashReporterForwarding);
    info->set_gather_indirectly_referenced_memory(
        kGatherIndirectlyReferencedMemory, kIndirectlyReferencedMemoryCap);
  }

  void ExpectCrashpadInfo(ProcessType process, bool is_64_bit) {
    ProcessMemoryNative memory;
    ASSERT_TRUE(memory.Initialize(process));

    ProcessMemoryRange range;
    ASSERT_TRUE(range.Initialize(&memory, is_64_bit));

    CrashpadInfo* info = CrashpadInfo::GetCrashpadInfo();

    CrashpadInfoReader reader;
    ASSERT_TRUE(reader.Initialize(&range, FromPointerCast<VMAddress>(info)));
    EXPECT_EQ(reader.CrashpadHandlerBehavior(), kCrashpadHandlerBehavior);
    EXPECT_EQ(reader.SystemCrashReporterForwarding(),
              kSystemCrashReporterForwarding);
    EXPECT_EQ(reader.GatherIndirectlyReferencedMemory(),
              kGatherIndirectlyReferencedMemory);
    EXPECT_EQ(reader.IndirectlyReferencedMemoryCap(),
              kIndirectlyReferencedMemoryCap);
    EXPECT_EQ(reader.ExtraMemoryRanges(),
              FromPointerCast<VMAddress>(&extra_memory_));
    EXPECT_EQ(reader.SimpleAnnotations(),
              FromPointerCast<VMAddress>(info->simple_annotations()));
    EXPECT_EQ(reader.AnnotationsList(),
              FromPointerCast<VMAddress>(info->annotations_list()));
  }

 private:
  SimpleAddressRangeBag extra_memory_;
  SimpleStringDictionary simple_annotations_;
  AnnotationList annotation_list_;

  DISALLOW_COPY_AND_ASSIGN(CrashpadInfoTest);
};

TEST(CrashpadInfoReader, ReadFromSelf) {
  CrashpadInfoTest test;

#if defined(ARCH_CPU_64_BITS)
  constexpr bool am_64_bit = true;
#else
  constexpr bool am_64_bit = false;
#endif

  test.ExpectCrashpadInfo(GetSelfProcess(), am_64_bit);
}

// TODO(scottmg): This needs to be be ported to use MultiprocessExec instead of
// relying on fork() to have the same pointers in parent and child.
#if !defined(OS_FUCHSIA)

class ReadFromChildTest : public Multiprocess {
 public:
  ReadFromChildTest() : Multiprocess(), info_test_() {}

  ~ReadFromChildTest() = default;

 private:
  void MultiprocessParent() {
#if defined(ARCH_CPU_64_BITS)
    constexpr bool am_64_bit = true;
#else
    constexpr bool am_64_bit = false;
#endif

    info_test_.ExpectCrashpadInfo(ChildPID(), am_64_bit);
  }

  void MultiprocessChild() { CheckedReadFileAtEOF(ReadPipeHandle()); }

  CrashpadInfoTest info_test_;

  DISALLOW_COPY_AND_ASSIGN(ReadFromChildTest);
};

TEST(CrashpadInfoReader, ReadFromChild) {
  ReadFromChildTest test;
  test.Run();
}

#endif  // !defined(OS_FUCHSIA)

}  // namespace
}  // namespace test
}  // namespace crashpad
