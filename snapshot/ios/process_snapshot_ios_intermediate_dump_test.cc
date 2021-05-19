// Copyright 2021 The Crashpad Authors. All rights reserved.
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

#include "snapshot/ios/process_snapshot_ios_intermediate_dump.h"

#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#include "base/stl_util.h"
#include "client/annotation.h"
#include "gtest/gtest.h"
#include "minidump/minidump_file_writer.h"
#include "test/errors.h"
#include "test/scoped_temp_dir.h"
#include "util/file/file_io.h"
#include "util/file/filesystem.h"
#include "util/file/string_file.h"
#include "util/misc/uuid.h"

namespace crashpad {
namespace test {
namespace {

using Key = internal::IntermediateDumpKey;
using internal::IOSIntermediateDumpWriter;
using internal::ProcessSnapshotIOSIntermediateDump;

class ReadToString : public crashpad::MemorySnapshot::Delegate {
 public:
  std::string result;

  bool MemorySnapshotDelegateRead(void* data, size_t size) override {
    result = std::string(reinterpret_cast<const char*>(data), size);
    return true;
  }
};

class ProcessSnapshotIOSIntermediateDumpTest : public testing::Test {
 protected:
  // testing::Test:

  void SetUp() override {
    path_ = temp_dir_.path().Append("dump_file");
    writer_ = std::make_unique<internal::IOSIntermediateDumpWriter>();
    EXPECT_TRUE(writer_->Open(path_));
    ASSERT_TRUE(IsRegularFile(path_));
  }

  void TearDown() override {
    writer_.reset();
    EXPECT_FALSE(IsRegularFile(path_));
  }

  const auto& path() const { return path_; }
  const auto& annotations() const { return annotations_; }
  auto writer() const { return writer_.get(); }

  bool DumpSnapshot(const ProcessSnapshotIOSIntermediateDump& snapshot) {
    MinidumpFileWriter minidump;
    minidump.InitializeFromSnapshot(&snapshot);
    StringFile string_file;
    return minidump.WriteEverything(&string_file);
  }

  void WriteProcessInfo(IOSIntermediateDumpWriter* writer) {
    IOSIntermediateDumpWriter::ScopedMap map(writer, Key::kProcessInfo);
    pid_t pid = 2;
    pid_t parent = 1;
    EXPECT_TRUE(writer->AddProperty(Key::kPID, &pid));
    EXPECT_TRUE(writer->AddProperty(Key::kParentPID, &parent));
    timeval start_time = {12, 0};
    EXPECT_TRUE(writer->AddProperty(Key::kStartTime, &start_time));

    time_value_t user_time = {20, 0};
    time_value_t system_time = {30, 0};
    {
      IOSIntermediateDumpWriter::ScopedMap taskInfo(writer,
                                                    Key::kTaskBasicInfo);
      EXPECT_TRUE(writer->AddProperty(Key::kUserTime, &user_time));
      EXPECT_TRUE(writer->AddProperty(Key::kSystemTime, &system_time));
    }
    {
      IOSIntermediateDumpWriter::ScopedMap taskThreadTimesMap(
          writer, Key::kTaskThreadTimes);
      writer->AddProperty(Key::kUserTime, &user_time);
      writer->AddProperty(Key::kSystemTime, &system_time);
    }

    timeval snapshot_time = {42, 0};
    writer->AddProperty(Key::kSnapshotTime, &snapshot_time);
  }

  void WriteSystemInfo(IOSIntermediateDumpWriter* writer) {
    IOSIntermediateDumpWriter::ScopedMap map(writer, Key::kSystemInfo);
    std::string machine_description = "Gibson";
    EXPECT_TRUE(writer->AddProperty(Key::kMachineDescription,
                                    machine_description.c_str(),
                                    machine_description.length()));
    int os_version_major = 1995;
    int os_version_minor = 9;
    int os_version_bugfix = 15;
    EXPECT_TRUE(writer->AddProperty(Key::kOSVersionMajor, &os_version_major));
    EXPECT_TRUE(writer->AddProperty(Key::kOSVersionMinor, &os_version_minor));
    EXPECT_TRUE(writer->AddProperty(Key::kOSVersionBugfix, &os_version_bugfix));
    std::string os_version_build = "Da Vinci";
    writer->AddProperty(Key::kOSVersionBuild,
                        os_version_build.c_str(),
                        os_version_build.length());

    int cpu_count = 1;
    EXPECT_TRUE(writer->AddProperty(Key::kCpuCount, &cpu_count));
    std::string cpu_vendor = "RISC";
    EXPECT_TRUE(writer->AddProperty(
        Key::kCpuVendor, cpu_vendor.c_str(), cpu_vendor.length()));

    bool has_daylight_saving_time = true;
    EXPECT_TRUE(writer->AddProperty(Key::kHasDaylightSavingTime,
                                    &has_daylight_saving_time));
    bool is_daylight_saving_time = true;
    EXPECT_TRUE(writer->AddProperty(Key::kIsDaylightSavingTime,
                                    &is_daylight_saving_time));
    int standard_offset_seconds = 7200;
    EXPECT_TRUE(writer->AddProperty(Key::kStandardOffsetSeconds,
                                    &standard_offset_seconds));
    int daylight_offset_seconds = 3600;
    EXPECT_TRUE(writer->AddProperty(Key::kDaylightOffsetSeconds,
                                    &daylight_offset_seconds));
    std::string standard_name = "Standard";
    EXPECT_TRUE(writer->AddProperty(
        Key::kStandardName, standard_name.c_str(), standard_name.length()));
    std::string daylight_name = "Daylight";
    EXPECT_TRUE(writer->AddProperty(
        Key::kDaylightName, daylight_name.c_str(), daylight_name.length()));

    vm_size_t page_size = getpagesize();
    EXPECT_TRUE(writer->AddProperty(Key::kPageSize, &page_size));
    {
      natural_t count = 0;
      IOSIntermediateDumpWriter::ScopedMap vmStatMap(writer, Key::kVMStat);
      EXPECT_TRUE(writer->AddProperty(Key::kActive, &count));
      EXPECT_TRUE(writer->AddProperty(Key::kInactive, &count));
      EXPECT_TRUE(writer->AddProperty(Key::kWired, &count));
      EXPECT_TRUE(writer->AddProperty(Key::kFree, &count));
    }
  }

  void WriteAnnotations(IOSIntermediateDumpWriter* writer) {
    constexpr char annotation_name[] = "annotation";
    constexpr char annotation_value[] = "annotation_value";
    {
      IOSIntermediateDumpWriter::ScopedArray annotationObjectArray(
          writer, Key::kAnnotationObjects);
      {
        IOSIntermediateDumpWriter::ScopedArrayMap annotationMap(writer);
        EXPECT_TRUE(writer->AddPropertyBytes(
            Key::kAnnotationName, annotation_name, strlen(annotation_name)));
        EXPECT_TRUE(writer->AddPropertyBytes(
            Key::kAnnotationValue, annotation_value, strlen(annotation_value)));
        Annotation::Type type = Annotation::Type::kString;
        EXPECT_TRUE(writer->AddProperty(Key::kAnnotationType, &type));
      }
    }
    {
      IOSIntermediateDumpWriter::ScopedArray annotationsSimpleArray(
          writer, Key::kAnnotationsSimpleMap);
      {
        IOSIntermediateDumpWriter::ScopedArrayMap annotationMap(writer);
        EXPECT_TRUE(writer->AddPropertyBytes(
            Key::kAnnotationName, annotation_name, strlen(annotation_name)));
        EXPECT_TRUE(writer->AddPropertyBytes(
            Key::kAnnotationValue, annotation_value, strlen(annotation_value)));
      }
    }

    IOSIntermediateDumpWriter::ScopedMap annotationMap(
        writer, Key::kAnnotationsCrashInfo);
    {
      EXPECT_TRUE(writer->AddPropertyBytes(Key::kAnnotationsCrashInfoMessage1,
                                           annotation_value,
                                           strlen(annotation_value)));
      EXPECT_TRUE(writer->AddPropertyBytes(Key::kAnnotationsCrashInfoMessage2,
                                           annotation_value,
                                           strlen(annotation_value)));
    }
  }

  void WriteModules(IOSIntermediateDumpWriter* writer) {
    IOSIntermediateDumpWriter::ScopedArray moduleArray(writer, Key::kModules);
    for (uint32_t image_index = 0; image_index < 10; ++image_index) {
      IOSIntermediateDumpWriter::ScopedArrayMap modules(writer);

      constexpr char image_file[] = "/path/to/module";
      EXPECT_TRUE(
          writer->AddProperty(Key::kName, image_file, strlen(image_file)));

      uint64_t address = 0;
      uint64_t vmsize = 0;
      uintptr_t imageFileModDate = 0;
      uint32_t current_version = 0;
      uint32_t filetype = 0;
      uint64_t source_version = 0;
      uint8_t uuid[16];
      EXPECT_TRUE(writer->AddProperty(Key::kAddress, &address));
      EXPECT_TRUE(writer->AddProperty(Key::kSize, &vmsize));
      EXPECT_TRUE(writer->AddProperty(Key::kTimestamp, &imageFileModDate));
      EXPECT_TRUE(
          writer->AddProperty(Key::kDylibCurrentVersion, &current_version));
      EXPECT_TRUE(writer->AddProperty(Key::kSourceVersion, &source_version));
      EXPECT_TRUE(writer->AddProperty(Key::kUUID, &uuid));
      EXPECT_TRUE(writer->AddProperty(Key::kFileType, &filetype));
      WriteAnnotations(writer);
    }
  }

  void WriteMachException(IOSIntermediateDumpWriter* writer) {
    IOSIntermediateDumpWriter::ScopedMap machExceptionMap(writer,
                                                          Key::kMachException);
    exception_type_t exception = 0;
    mach_exception_data_type_t code[] = {0, 0};
    mach_msg_type_number_t code_count = 2;

#if defined(ARCH_CPU_X86_64)
    thread_state_flavor_t flavor = x86_THREAD_STATE64;
    x86_thread_state_t state = {};
    state.tsh.flavor = flavor;
    mach_msg_type_number_t state_count = x86_THREAD_STATE64_COUNT;
    size_t state_length = sizeof(x86_thread_state_t);
#elif defined(ARCH_CPU_ARM64)
    thread_state_flavor_t flavor = ARM_THREAD_STATE64;
    arm_unified_thread_state_t state = {};
    state.ash.flavor = flavor;
    mach_msg_type_number_t state_count = ARM_THREAD_STATE64_COUNT;
    size_t state_length = sizeof(arm_unified_thread_state_t);
#endif
    EXPECT_TRUE(writer->AddProperty(Key::kException, &exception));
    EXPECT_TRUE(writer->AddProperty(Key::kCode, code, code_count));
    EXPECT_TRUE(writer->AddProperty(Key::kCodeCount, &code_count));
    EXPECT_TRUE(writer->AddProperty(Key::kFlavor, &flavor));
    EXPECT_TRUE(writer->AddPropertyBytes(
        Key::kState, reinterpret_cast<const void*>(&state), state_length));
    EXPECT_TRUE(writer->AddProperty(Key::kStateCount, &state_count));
    uint64_t thread_id = 1;
    EXPECT_TRUE(writer->AddProperty(Key::kThreadID, &thread_id));
  }

  void WriteThreads(IOSIntermediateDumpWriter* writer) {
    vm_address_t stack_region_address = 0;
    IOSIntermediateDumpWriter::ScopedArray threadArray(writer, Key::kThreads);
    for (uint64_t thread_id = 1; thread_id < 10; thread_id++) {
      IOSIntermediateDumpWriter::ScopedArrayMap threadMap(writer);
      EXPECT_TRUE(writer->AddProperty(Key::kThreadID, &thread_id));

      integer_t suspend_count = 666;
      integer_t importance = 5;
      uint64_t thread_handle = thread_id;
      EXPECT_TRUE(writer->AddProperty(Key::kSuspendCount, &suspend_count));
      EXPECT_TRUE(writer->AddProperty(Key::kPriority, &importance));
      EXPECT_TRUE(writer->AddProperty(Key::kThreadDataAddress, &thread_handle));

#if defined(ARCH_CPU_X86_64)
      x86_thread_state64_t thread_state = {};
      x86_float_state64_t float_state = {};
      x86_debug_state64_t debug_state = {};
#elif defined(ARCH_CPU_ARM64)
      arm_thread_state64_t thread_state = {};
      arm_neon_state64_t float_state = {};
      arm_debug_state64_t debug_state = {};
#endif

      EXPECT_TRUE(writer->AddProperty(Key::kThreadState, &thread_state));
      EXPECT_TRUE(writer->AddProperty(Key::kFloatState, &float_state));
      EXPECT_TRUE(writer->AddProperty(Key::kDebugState, &debug_state));

      // Non-overlapping stack_region_address.
      stack_region_address += 10;
      EXPECT_TRUE(
          writer->AddProperty(Key::kStackRegionAddress, &stack_region_address));
      constexpr char memory_region[] = "stack_data";
      EXPECT_TRUE(
          writer->AddPropertyBytes(Key::kStackRegionData, memory_region, 10));
      {
        IOSIntermediateDumpWriter::ScopedArray memoryRegions(
            writer, Key::kThreadContextMemoryRegions);
        {
          IOSIntermediateDumpWriter::ScopedArrayMap memoryRegion(writer);
          const vm_address_t memory_region_address = 0;
          EXPECT_TRUE(writer->AddProperty(
              Key::kThreadContextMemoryRegionAddress, &memory_region_address));
          constexpr char memory_region[] = "string";
          EXPECT_TRUE(writer->AddPropertyBytes(
              Key::kThreadContextMemoryRegionData, memory_region, 6));
        }
      }
    }
  }

  void ExpectThreads(const std::vector<const ThreadSnapshot*>& threads) {
    uint64_t thread_id = 1;
    for (auto thread : threads) {
      EXPECT_EQ(thread->ThreadID(), thread_id);
      EXPECT_EQ(thread->SuspendCount(), 666);
      EXPECT_EQ(thread->Priority(), 5);
      EXPECT_EQ(thread->ThreadSpecificDataAddress(), thread_id++);
      ReadToString delegate;
      for (auto memory : thread->ExtraMemory()) {
        memory->Read(&delegate);
        EXPECT_STREQ(delegate.result.c_str(), "string");
      }
      thread->Stack()->Read(&delegate);
      EXPECT_STREQ(delegate.result.c_str(), "stack_data");
    }
    //    const CPUContext* Context() const override;
  }

  void ExpectSystem(const SystemSnapshot& system) {
    EXPECT_EQ(system.CPUCount(), 1u);
    EXPECT_STREQ(system.CPUVendor().c_str(), "RISC");
    int major;
    int minor;
    int bugfix;
    std::string build;
    system.OSVersion(&major, &minor, &bugfix, &build);
    EXPECT_EQ(major, 1995);
    EXPECT_EQ(minor, 9);
    EXPECT_EQ(bugfix, 15);
    EXPECT_STREQ(build.c_str(), "Da Vinci");
    EXPECT_STREQ(system.OSVersionFull().c_str(), "1995.9.15 Da Vinci");
    EXPECT_STREQ(system.MachineDescription().c_str(), "Gibson");

    SystemSnapshot::DaylightSavingTimeStatus dst_status;
    int standard_offset_seconds;
    int daylight_offset_seconds;
    std::string standard_name;
    std::string daylight_name;

    system.TimeZone(&dst_status,
                    &standard_offset_seconds,
                    &daylight_offset_seconds,
                    &standard_name,
                    &daylight_name);
    EXPECT_EQ(standard_offset_seconds, 7200);
    EXPECT_EQ(daylight_offset_seconds, 3600);
    EXPECT_STREQ(standard_name.c_str(), "Standard");
    EXPECT_STREQ(daylight_name.c_str(), "Daylight");
  }

  void ExpectSnapshot(const ProcessSnapshot& snapshot) {
    EXPECT_EQ(snapshot.ProcessID(), 2);
    EXPECT_EQ(snapshot.ParentProcessID(), 1);

    timeval snapshot_time;
    snapshot.SnapshotTime(&snapshot_time);
    EXPECT_EQ(snapshot_time.tv_sec, 42);
    EXPECT_EQ(snapshot_time.tv_usec, 0);

    timeval start_time;
    snapshot.ProcessStartTime(&start_time);
    EXPECT_EQ(start_time.tv_sec, 12);
    EXPECT_EQ(start_time.tv_usec, 0);

    timeval user_time, system_time;
    snapshot.ProcessCPUTimes(&user_time, &system_time);
    EXPECT_EQ(user_time.tv_sec, 40);
    EXPECT_EQ(user_time.tv_usec, 0);
    EXPECT_EQ(system_time.tv_sec, 60);
    EXPECT_EQ(system_time.tv_usec, 0);

    ExpectSystem(*snapshot.System());
    ExpectThreads(snapshot.Threads());
    //    ExpectModules(*snapshot.Modules());
    //    ExpectException(*snapshot.Exception());
    //    ExpectAnnotationsSimpleMap(snapshot.AnnotationsSimpleMap());
  }

 private:
  std::unique_ptr<internal::IOSIntermediateDumpWriter> writer_;
  ScopedTempDir temp_dir_;
  base::FilePath path_;
  std::map<std::string, std::string> annotations_;
};

TEST_F(ProcessSnapshotIOSIntermediateDumpTest, InitializeNoFile) {
  const base::FilePath file;
  ProcessSnapshotIOSIntermediateDump process_snapshot;
  EXPECT_FALSE(process_snapshot.Initialize(file, annotations()));
  EXPECT_TRUE(LoggingRemoveFile(path()));
  EXPECT_FALSE(IsRegularFile(path()));
}

TEST_F(ProcessSnapshotIOSIntermediateDumpTest, InitializeEmpty) {
  ProcessSnapshotIOSIntermediateDump process_snapshot;
  EXPECT_FALSE(process_snapshot.Initialize(path(), annotations()));
  EXPECT_FALSE(IsRegularFile(path()));
}

TEST_F(ProcessSnapshotIOSIntermediateDumpTest, InitializeMinimumDump) {
  {
    IOSIntermediateDumpWriter::ScopedRootMap rootMap(writer());
    uint8_t version = 1;
    EXPECT_TRUE(writer()->AddProperty(Key::kVersion, &version));
    { IOSIntermediateDumpWriter::ScopedMap map(writer(), Key::kSystemInfo); }
    { IOSIntermediateDumpWriter::ScopedMap map(writer(), Key::kProcessInfo); }
  }
  ProcessSnapshotIOSIntermediateDump process_snapshot;
  ASSERT_TRUE(process_snapshot.Initialize(path(), annotations()));
  EXPECT_FALSE(IsRegularFile(path()));
  EXPECT_TRUE(DumpSnapshot(process_snapshot));
}

TEST_F(ProcessSnapshotIOSIntermediateDumpTest, MissingSystemDump) {
  {
    IOSIntermediateDumpWriter::ScopedRootMap rootMap(writer());
    uint8_t version = 1;
    EXPECT_TRUE(writer()->AddProperty(Key::kVersion, &version));
    { IOSIntermediateDumpWriter::ScopedMap map(writer(), Key::kProcessInfo); }
  }
  ProcessSnapshotIOSIntermediateDump process_snapshot;
  ASSERT_FALSE(process_snapshot.Initialize(path(), annotations()));
  EXPECT_FALSE(IsRegularFile(path()));
}

TEST_F(ProcessSnapshotIOSIntermediateDumpTest, MissingProcessDump) {
  {
    IOSIntermediateDumpWriter::ScopedRootMap rootMap(writer());
    uint8_t version = 1;
    EXPECT_TRUE(writer()->AddProperty(Key::kVersion, &version));
    { IOSIntermediateDumpWriter::ScopedMap map(writer(), Key::kSystemInfo); }
  }
  ProcessSnapshotIOSIntermediateDump process_snapshot;
  ASSERT_FALSE(process_snapshot.Initialize(path(), annotations()));
  EXPECT_FALSE(IsRegularFile(path()));
}

TEST_F(ProcessSnapshotIOSIntermediateDumpTest, EmptySignalDump) {
  {
    IOSIntermediateDumpWriter::ScopedRootMap rootMap(writer());
    uint8_t version = 1;
    EXPECT_TRUE(writer()->AddProperty(Key::kVersion, &version));
    WriteSystemInfo(writer());
    WriteProcessInfo(writer());
    {
      IOSIntermediateDumpWriter::ScopedMap map(writer(), Key::kSignalException);
      uint64_t thread_id = 1;
      EXPECT_TRUE(writer()->AddProperty(Key::kThreadID, &thread_id));
    }
    {
      IOSIntermediateDumpWriter::ScopedArray threadArray(writer(),
                                                         Key::kThreads);
      IOSIntermediateDumpWriter::ScopedArrayMap threadMap(writer());
      uint64_t thread_id = 1;
      writer()->AddProperty(Key::kThreadID, &thread_id);
    }
  }
  ProcessSnapshotIOSIntermediateDump process_snapshot;
  ASSERT_TRUE(process_snapshot.Initialize(path(), annotations()));
  EXPECT_FALSE(IsRegularFile(path()));
  EXPECT_TRUE(DumpSnapshot(process_snapshot));
}

TEST_F(ProcessSnapshotIOSIntermediateDumpTest, EmptyMachDump) {
  {
    IOSIntermediateDumpWriter::ScopedRootMap rootMap(writer());
    uint8_t version = 1;
    EXPECT_TRUE(writer()->AddProperty(Key::kVersion, &version));
    WriteSystemInfo(writer());
    WriteProcessInfo(writer());
    {
      IOSIntermediateDumpWriter::ScopedMap map(writer(), Key::kMachException);
      uint64_t thread_id = 1;
      EXPECT_TRUE(writer()->AddProperty(Key::kThreadID, &thread_id));
    }
    {
      IOSIntermediateDumpWriter::ScopedArray threadArray(writer(),
                                                         Key::kThreads);
      IOSIntermediateDumpWriter::ScopedArrayMap threadMap(writer());
      uint64_t thread_id = 1;
      writer()->AddProperty(Key::kThreadID, &thread_id);
    }
  }
  ProcessSnapshotIOSIntermediateDump process_snapshot;
  ASSERT_TRUE(process_snapshot.Initialize(path(), annotations()));
  EXPECT_FALSE(IsRegularFile(path()));
  EXPECT_TRUE(DumpSnapshot(process_snapshot));
}

TEST_F(ProcessSnapshotIOSIntermediateDumpTest, EmptyExceptionDump) {
  {
    IOSIntermediateDumpWriter::ScopedRootMap rootMap(writer());
    uint8_t version = 1;
    EXPECT_TRUE(writer()->AddProperty(Key::kVersion, &version));
    WriteSystemInfo(writer());
    WriteProcessInfo(writer());
    {
      IOSIntermediateDumpWriter::ScopedMap map(writer(), Key::kNSException);
      uint64_t thread_id = 1;
      EXPECT_TRUE(writer()->AddProperty(Key::kThreadID, &thread_id));
    }
    {
      IOSIntermediateDumpWriter::ScopedArray threadArray(writer(),
                                                         Key::kThreads);
      IOSIntermediateDumpWriter::ScopedArrayMap threadMap(writer());
      uint64_t thread_id = 1;
      writer()->AddProperty(Key::kThreadID, &thread_id);
    }
  }
  ProcessSnapshotIOSIntermediateDump process_snapshot;
  ASSERT_TRUE(process_snapshot.Initialize(path(), annotations()));
  EXPECT_FALSE(IsRegularFile(path()));
  EXPECT_TRUE(DumpSnapshot(process_snapshot));
}

TEST_F(ProcessSnapshotIOSIntermediateDumpTest, EmptyUncaughtNSExceptionDump) {
  {
    IOSIntermediateDumpWriter::ScopedRootMap rootMap(writer());
    uint8_t version = 1;
    EXPECT_TRUE(writer()->AddProperty(Key::kVersion, &version));
    WriteSystemInfo(writer());
    WriteProcessInfo(writer());
    {
      IOSIntermediateDumpWriter::ScopedMap map(writer(), Key::kNSException);
      uint64_t thread_id = 1;
      EXPECT_TRUE(writer()->AddProperty(Key::kThreadID, &thread_id));
    }
    {
      IOSIntermediateDumpWriter::ScopedArray threadArray(writer(),
                                                         Key::kThreads);
      IOSIntermediateDumpWriter::ScopedArrayMap threadMap(writer());
      uint64_t thread_id = 1;
      writer()->AddProperty(Key::kThreadID, &thread_id);
      const uint64_t frames[] = {0, 0};
      const size_t num_frames = 2;
      writer()->AddProperty(
          Key::kThreadUncaughtNSExceptionFrames, frames, num_frames);
    }
  }
  ProcessSnapshotIOSIntermediateDump process_snapshot;
  ASSERT_TRUE(process_snapshot.Initialize(path(), annotations()));
  EXPECT_FALSE(IsRegularFile(path()));
  EXPECT_TRUE(DumpSnapshot(process_snapshot));
}

TEST_F(ProcessSnapshotIOSIntermediateDumpTest, FullReport) {
  {
    IOSIntermediateDumpWriter::ScopedRootMap rootMap(writer());
    uint8_t version = 1;
    EXPECT_TRUE(writer()->AddProperty(Key::kVersion, &version));
    WriteSystemInfo(writer());
    WriteProcessInfo(writer());
    WriteThreads(writer());
    WriteModules(writer());
    WriteMachException(writer());
  }
  ProcessSnapshotIOSIntermediateDump process_snapshot;
  ASSERT_TRUE(process_snapshot.Initialize(path(), annotations()));
  EXPECT_FALSE(IsRegularFile(path()));
  EXPECT_TRUE(DumpSnapshot(process_snapshot));
  ExpectSnapshot(process_snapshot);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
