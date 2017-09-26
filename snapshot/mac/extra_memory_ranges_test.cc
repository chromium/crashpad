// Copyright 2016 The Crashpad Authors. All rights reserved.
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

#include "snapshot/mac/module_snapshot_mac.h"

#include <stdlib.h>
#include <string.h>

#include <string>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "client/crashpad_info.h"
#include "client/simple_address_range_bag.h"
#include "gtest/gtest.h"
#include "test/mac/mach_multiprocess.h"
#include "snapshot/mac/process_snapshot_mac.h"
#include "test/test_paths.h"
#include "util/file/file_io.h"
#include "util/mac/mac_util.h"

namespace crashpad {
namespace test {
namespace {

class TestMemoryRanges final
    : public MachMultiprocess {
 public:

  explicit TestMemoryRanges()
      : MachMultiprocess() {
  }

  ~TestMemoryRanges() {}

 private:
  // MachMultiprocess:

  void MachMultiprocessParent() override {
    // Wait for the child process to indicate that it’s done setting up its
    // annotations via the CrashpadInfo interface.
    char c;
    CheckedReadFileExactly(ReadPipeHandle(), &c, sizeof(c));

    ProcessSnapshotMac snapshot;
    ASSERT_TRUE(snapshot.Initialize(ChildTask()));

    // Verify the extra memory ranges set via the CrashpadInfo interface.
    std::set<CheckedRange<uint64_t>> all_ranges;
    for (const auto* module : snapshot.Modules()) {
      for (const auto& range : module->ExtraMemoryRanges())
        all_ranges.insert(range);
    }

    EXPECT_EQ(all_ranges.size(), 5u);
    EXPECT_NE(all_ranges.find(CheckedRange<uint64_t>(0, 1)), all_ranges.end());
    EXPECT_NE(all_ranges.find(CheckedRange<uint64_t>(1, 0)), all_ranges.end());
    EXPECT_NE(all_ranges.find(CheckedRange<uint64_t>(1234, 5678)),
              all_ranges.end());
    EXPECT_NE(all_ranges.find(CheckedRange<uint64_t>(0x1000000000ULL, 0x1000)),
              all_ranges.end());
    EXPECT_NE(all_ranges.find(CheckedRange<uint64_t>(0x2000, 0x2000000000ULL)),
              all_ranges.end());

    // Tell the child process to continue.
    CheckedWriteFile(WritePipeHandle(), &c, sizeof(c));
  }

  void MachMultiprocessChild() override {
    CrashpadInfo* crashpad_info = CrashpadInfo::GetCrashpadInfo();

    // This is "leaked" to crashpad_info.
    SimpleAddressRangeBag* extra_ranges = new SimpleAddressRangeBag();
    extra_ranges->Insert(CheckedRange<uint64_t>(0, 1));
    extra_ranges->Insert(CheckedRange<uint64_t>(1, 0));
    extra_ranges->Insert(CheckedRange<uint64_t>(0x1000000000ULL, 0x1000));
    extra_ranges->Insert(CheckedRange<uint64_t>(0x2000, 0x2000000000ULL));
    extra_ranges->Insert(CheckedRange<uint64_t>(1234, 5678));
    extra_ranges->Insert(CheckedRange<uint64_t>(1234, 5678));
    extra_ranges->Insert(CheckedRange<uint64_t>(1234, 5678));
    crashpad_info->set_extra_memory_ranges(extra_ranges);

    // Tell the parent that the environment has been set up.
    char c = '\0';
    CheckedWriteFile(WritePipeHandle(), &c, sizeof(c));

    // Wait for the parent to indicate that it’s safe to crash.
    CheckedReadFileExactly(ReadPipeHandle(), &c, sizeof(c));
  }

  DISALLOW_COPY_AND_ASSIGN(TestMemoryRanges);
};

TEST(ExtraMemoryRanges, DontCrash) {
  TestMemoryRanges test_memory_ranges;
  test_memory_ranges.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
