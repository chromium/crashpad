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
#include "test/multiprocess_exec.h"
#include "test/test_paths.h"
#include "util/file/file_io.h"
#include "util/misc/from_pointer_cast.h"

#if defined(OS_FUCHSIA)
#include <zircon/process.h>

#include "util/process/process_memory_fuchsia.h"
#elif defined(OS_POSIX)
#include "util/process/process_memory_linux.h"
#else
#error Port.
#endif

namespace crashpad {
namespace test {
namespace {

#if defined(OS_FUCHSIA)
ProcessHandle GetSelf() { return zx_process_self(); }
#elif defined(OS_POSIX)
ProcessHandle GetSelf() { return getpid(); }
#else
#error Port.
#endif

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

  void ExpectCrashpadInfo(ProcessHandle process,
                          bool is_64_bit,
                          VMAddress info_address,
                          VMAddress simple_annotations_address,
                          VMAddress annotations_list_address) {
#if defined(OS_FUCHSIA)
    ProcessMemoryFuchsia memory;
#elif defined(OS_POSIX)
    ProcessMemoryLinux memory;
#else
#error Port.
#endif
    ASSERT_TRUE(memory.Initialize(process));

    ProcessMemoryRange range;
    ASSERT_TRUE(range.Initialize(&memory, is_64_bit));

    CrashpadInfoReader reader;
    ASSERT_TRUE(reader.Initialize(&range, info_address));
    EXPECT_EQ(reader.CrashpadHandlerBehavior(), kCrashpadHandlerBehavior);
    EXPECT_EQ(reader.SystemCrashReporterForwarding(),
              kSystemCrashReporterForwarding);
    EXPECT_EQ(reader.GatherIndirectlyReferencedMemory(),
              kGatherIndirectlyReferencedMemory);
    EXPECT_EQ(reader.IndirectlyReferencedMemoryCap(),
              kIndirectlyReferencedMemoryCap);
    EXPECT_EQ(reader.ExtraMemoryRanges(),
              FromPointerCast<VMAddress>(&extra_memory_));
    EXPECT_EQ(reader.SimpleAnnotations(), simple_annotations_address);
    EXPECT_EQ(reader.AnnotationsList(), annotations_list_address);
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

  CrashpadInfo* info = CrashpadInfo::GetCrashpadInfo();
  test.ExpectCrashpadInfo(
      GetSelf(),
      am_64_bit,
      FromPointerCast<VMAddress>(info),
      FromPointerCast<VMAddress>(info->simple_annotations()),
      FromPointerCast<VMAddress>(info->annotations_list()));
}

class ReadFromChildTest : public MultiprocessExec {
 public:
  ReadFromChildTest() : MultiprocessExec(), info_test_() {}

  ~ReadFromChildTest() = default;

 private:
  void MultiprocessParent() {
#if defined(ARCH_CPU_64_BITS)
    constexpr bool am_64_bit = true;
#else
    constexpr bool am_64_bit = false;
#endif

    VMAddress info, simple_annotations, annotations_list;
    CheckedReadFileExactly(ReadPipeHandle(), &info, sizeof(info));
    CheckedReadFileExactly(
        ReadPipeHandle(), &simple_annotations, sizeof(simple_annotations));
    CheckedReadFileExactly(
        ReadPipeHandle(), &annotations_list, sizeof(annotations_list));
    info_test_.ExpectCrashpadInfo(GetChildHandle(),
                                  am_64_bit,
                                  info,
                                  simple_annotations,
                                  annotations_list);
  }

  CrashpadInfoTest info_test_;

  DISALLOW_COPY_AND_ASSIGN(ReadFromChildTest);
};

TEST(CrashpadInfoReader, DISABLED_CHILD_ReadFromChild) {
  CrashpadInfo* info = CrashpadInfo::GetCrashpadInfo();
  auto* simple_annotations  = info->simple_annotations();
  auto* annotations_list  = info->annotations_list();
  CheckedWriteFile(STDOUT_FILENO, &info, sizeof(info));
  CheckedWriteFile(
      STDOUT_FILENO, &simple_annotations, sizeof(simple_annotations));
  CheckedWriteFile(STDOUT_FILENO, &annotations_list, sizeof(annotations_list));
  sleep(1);
  CheckedReadFileAtEOF(STDIN_FILENO);
}

TEST(CrashpadInfoReader, ReadFromChild) {
  ReadFromChildTest test;

  std::vector<std::string> args;
  args.push_back(
      "--gtest_filter=CrashpadInfoReader.DISABLED_CHILD_ReadFromChild");
  args.push_back("--gtest_also_run_disabled_tests");
  test.SetChildCommand(TestPaths::BuildArtifact(
                           "snapshot", "", TestPaths::FileType::kExecutable),
                       &args);
  test.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
