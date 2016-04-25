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

#include "minidump/minidump_file_writer.h"

#include <stdint.h>
#include <string.h>

#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "gtest/gtest.h"
#include "minidump/minidump_stream_writer.h"
#include "minidump/minidump_writable.h"
#include "minidump/test/minidump_file_writer_test_util.h"
#include "minidump/test/minidump_writable_test_util.h"
#include "snapshot/test/test_cpu_context.h"
#include "snapshot/test/test_exception_snapshot.h"
#include "snapshot/test/test_memory_snapshot.h"
#include "snapshot/test/test_module_snapshot.h"
#include "snapshot/test/test_process_snapshot.h"
#include "snapshot/test/test_system_snapshot.h"
#include "snapshot/test/test_thread_snapshot.h"
#include "test/gtest_death_check.h"
#include "util/file/string_file.h"

namespace crashpad {
namespace test {
namespace {

TEST(MinidumpFileWriter, Empty) {
  MinidumpFileWriter minidump_file;
  StringFile string_file;
  ASSERT_TRUE(minidump_file.WriteEverything(&string_file));
  ASSERT_EQ(sizeof(MINIDUMP_HEADER), string_file.string().size());

  const MINIDUMP_DIRECTORY* directory;
  const MINIDUMP_HEADER* header =
      MinidumpHeaderAtStart(string_file.string(), &directory);
  ASSERT_NO_FATAL_FAILURE(VerifyMinidumpHeader(header, 0, 0));
  EXPECT_FALSE(directory);
}

class TestStream final : public internal::MinidumpStreamWriter {
 public:
  TestStream(MinidumpStreamType stream_type,
             size_t stream_size,
             uint8_t stream_value)
      : stream_data_(stream_size, stream_value), stream_type_(stream_type) {}

  ~TestStream() override {}

  // MinidumpStreamWriter:
  MinidumpStreamType StreamType() const override {
    return stream_type_;
  }

 protected:
  // MinidumpWritable:
  size_t SizeOfObject() override {
    EXPECT_GE(state(), kStateFrozen);
    return stream_data_.size();
  }

  bool WriteObject(FileWriterInterface* file_writer) override {
    EXPECT_EQ(state(), kStateWritable);
    return file_writer->Write(&stream_data_[0], stream_data_.size());
  }

 private:
  std::string stream_data_;
  MinidumpStreamType stream_type_;

  DISALLOW_COPY_AND_ASSIGN(TestStream);
};

TEST(MinidumpFileWriter, OneStream) {
  MinidumpFileWriter minidump_file;
  const time_t kTimestamp = 0x155d2fb8;
  minidump_file.SetTimestamp(kTimestamp);

  const size_t kStreamSize = 5;
  const MinidumpStreamType kStreamType = static_cast<MinidumpStreamType>(0x4d);
  const uint8_t kStreamValue = 0x5a;
  auto stream =
      base::WrapUnique(new TestStream(kStreamType, kStreamSize, kStreamValue));
  minidump_file.AddStream(std::move(stream));

  StringFile string_file;
  ASSERT_TRUE(minidump_file.WriteEverything(&string_file));

  const size_t kDirectoryOffset = sizeof(MINIDUMP_HEADER);
  const size_t kStreamOffset = kDirectoryOffset + sizeof(MINIDUMP_DIRECTORY);
  const size_t kFileSize = kStreamOffset + kStreamSize;

  ASSERT_EQ(kFileSize, string_file.string().size());

  const MINIDUMP_DIRECTORY* directory;
  const MINIDUMP_HEADER* header =
      MinidumpHeaderAtStart(string_file.string(), &directory);
  ASSERT_NO_FATAL_FAILURE(VerifyMinidumpHeader(header, 1, kTimestamp));
  ASSERT_TRUE(directory);

  EXPECT_EQ(kStreamType, directory[0].StreamType);
  EXPECT_EQ(kStreamSize, directory[0].Location.DataSize);
  EXPECT_EQ(kStreamOffset, directory[0].Location.Rva);

  const uint8_t* stream_data = MinidumpWritableAtLocationDescriptor<uint8_t>(
      string_file.string(), directory[0].Location);
  ASSERT_TRUE(stream_data);

  std::string expected_stream(kStreamSize, kStreamValue);
  EXPECT_EQ(0, memcmp(stream_data, expected_stream.c_str(), kStreamSize));
}

TEST(MinidumpFileWriter, ThreeStreams) {
  MinidumpFileWriter minidump_file;
  const time_t kTimestamp = 0x155d2fb8;
  minidump_file.SetTimestamp(kTimestamp);

  const size_t kStream0Size = 5;
  const MinidumpStreamType kStream0Type = static_cast<MinidumpStreamType>(0x6d);
  const uint8_t kStream0Value = 0x5a;
  auto stream0 = base::WrapUnique(
      new TestStream(kStream0Type, kStream0Size, kStream0Value));
  minidump_file.AddStream(std::move(stream0));

  // Make the second stream’s type be a smaller quantity than the first stream’s
  // to test that the streams show up in the order that they were added, not in
  // numeric order.
  const size_t kStream1Size = 3;
  const MinidumpStreamType kStream1Type = static_cast<MinidumpStreamType>(0x4d);
  const uint8_t kStream1Value = 0xa5;
  auto stream1 = base::WrapUnique(
      new TestStream(kStream1Type, kStream1Size, kStream1Value));
  minidump_file.AddStream(std::move(stream1));

  const size_t kStream2Size = 1;
  const MinidumpStreamType kStream2Type = static_cast<MinidumpStreamType>(0x7e);
  const uint8_t kStream2Value = 0x36;
  auto stream2 = base::WrapUnique(
      new TestStream(kStream2Type, kStream2Size, kStream2Value));
  minidump_file.AddStream(std::move(stream2));

  StringFile string_file;
  ASSERT_TRUE(minidump_file.WriteEverything(&string_file));

  const size_t kDirectoryOffset = sizeof(MINIDUMP_HEADER);
  const size_t kStream0Offset =
      kDirectoryOffset + 3 * sizeof(MINIDUMP_DIRECTORY);
  const size_t kStream1Padding = 3;
  const size_t kStream1Offset = kStream0Offset + kStream0Size + kStream1Padding;
  const size_t kStream2Padding = 1;
  const size_t kStream2Offset = kStream1Offset + kStream1Size + kStream2Padding;
  const size_t kFileSize = kStream2Offset + kStream2Size;

  ASSERT_EQ(kFileSize, string_file.string().size());

  const MINIDUMP_DIRECTORY* directory;
  const MINIDUMP_HEADER* header =
      MinidumpHeaderAtStart(string_file.string(), &directory);
  ASSERT_NO_FATAL_FAILURE(VerifyMinidumpHeader(header, 3, kTimestamp));
  ASSERT_TRUE(directory);

  EXPECT_EQ(kStream0Type, directory[0].StreamType);
  EXPECT_EQ(kStream0Size, directory[0].Location.DataSize);
  EXPECT_EQ(kStream0Offset, directory[0].Location.Rva);
  EXPECT_EQ(kStream1Type, directory[1].StreamType);
  EXPECT_EQ(kStream1Size, directory[1].Location.DataSize);
  EXPECT_EQ(kStream1Offset, directory[1].Location.Rva);
  EXPECT_EQ(kStream2Type, directory[2].StreamType);
  EXPECT_EQ(kStream2Size, directory[2].Location.DataSize);
  EXPECT_EQ(kStream2Offset, directory[2].Location.Rva);

  const uint8_t* stream0_data = MinidumpWritableAtLocationDescriptor<uint8_t>(
      string_file.string(), directory[0].Location);
  ASSERT_TRUE(stream0_data);

  std::string expected_stream0(kStream0Size, kStream0Value);
  EXPECT_EQ(0, memcmp(stream0_data, expected_stream0.c_str(), kStream0Size));

  const int kZeroes[16] = {};
  ASSERT_GE(sizeof(kZeroes), kStream1Padding);
  EXPECT_EQ(0, memcmp(stream0_data + kStream0Size, kZeroes, kStream1Padding));

  const uint8_t* stream1_data = MinidumpWritableAtLocationDescriptor<uint8_t>(
      string_file.string(), directory[1].Location);
  ASSERT_TRUE(stream1_data);

  std::string expected_stream1(kStream1Size, kStream1Value);
  EXPECT_EQ(0, memcmp(stream1_data, expected_stream1.c_str(), kStream1Size));

  ASSERT_GE(sizeof(kZeroes), kStream2Padding);
  EXPECT_EQ(0, memcmp(stream1_data + kStream1Size, kZeroes, kStream2Padding));

  const uint8_t* stream2_data = MinidumpWritableAtLocationDescriptor<uint8_t>(
      string_file.string(), directory[2].Location);
  ASSERT_TRUE(stream2_data);

  std::string expected_stream2(kStream2Size, kStream2Value);
  EXPECT_EQ(0, memcmp(stream2_data, expected_stream2.c_str(), kStream2Size));
}

TEST(MinidumpFileWriter, ZeroLengthStream) {
  MinidumpFileWriter minidump_file;

  const size_t kStreamSize = 0;
  const MinidumpStreamType kStreamType = static_cast<MinidumpStreamType>(0x4d);
  auto stream = base::WrapUnique(new TestStream(kStreamType, kStreamSize, 0));
  minidump_file.AddStream(std::move(stream));

  StringFile string_file;
  ASSERT_TRUE(minidump_file.WriteEverything(&string_file));

  const size_t kDirectoryOffset = sizeof(MINIDUMP_HEADER);
  const size_t kStreamOffset = kDirectoryOffset + sizeof(MINIDUMP_DIRECTORY);
  const size_t kFileSize = kStreamOffset + kStreamSize;

  ASSERT_EQ(kFileSize, string_file.string().size());

  const MINIDUMP_DIRECTORY* directory;
  const MINIDUMP_HEADER* header =
      MinidumpHeaderAtStart(string_file.string(), &directory);
  ASSERT_NO_FATAL_FAILURE(VerifyMinidumpHeader(header, 1, 0));
  ASSERT_TRUE(directory);

  EXPECT_EQ(kStreamType, directory[0].StreamType);
  EXPECT_EQ(kStreamSize, directory[0].Location.DataSize);
  EXPECT_EQ(kStreamOffset, directory[0].Location.Rva);
}

TEST(MinidumpFileWriter, InitializeFromSnapshot_Basic) {
  const uint32_t kSnapshotTime = 0x4976043c;
  const timeval kSnapshotTimeval = { static_cast<time_t>(kSnapshotTime), 0 };

  TestProcessSnapshot process_snapshot;
  process_snapshot.SetSnapshotTime(kSnapshotTimeval);

  auto system_snapshot = base::WrapUnique(new TestSystemSnapshot());
  system_snapshot->SetCPUArchitecture(kCPUArchitectureX86_64);
  system_snapshot->SetOperatingSystem(SystemSnapshot::kOperatingSystemMacOSX);
  process_snapshot.SetSystem(std::move(system_snapshot));

  auto peb_snapshot = base::WrapUnique(new TestMemorySnapshot());
  const uint64_t kPebAddress = 0x07f90000;
  peb_snapshot->SetAddress(kPebAddress);
  const size_t kPebSize = 0x280;
  peb_snapshot->SetSize(kPebSize);
  peb_snapshot->SetValue('p');
  process_snapshot.AddExtraMemory(std::move(peb_snapshot));

  MinidumpFileWriter minidump_file_writer;
  minidump_file_writer.InitializeFromSnapshot(&process_snapshot);

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  const MINIDUMP_DIRECTORY* directory;
  const MINIDUMP_HEADER* header =
      MinidumpHeaderAtStart(string_file.string(), &directory);
  ASSERT_NO_FATAL_FAILURE(VerifyMinidumpHeader(header, 5, kSnapshotTime));
  ASSERT_TRUE(directory);

  EXPECT_EQ(kMinidumpStreamTypeSystemInfo, directory[0].StreamType);
  EXPECT_TRUE(MinidumpWritableAtLocationDescriptor<MINIDUMP_SYSTEM_INFO>(
                  string_file.string(), directory[0].Location));

  EXPECT_EQ(kMinidumpStreamTypeMiscInfo, directory[1].StreamType);
  EXPECT_TRUE(MinidumpWritableAtLocationDescriptor<MINIDUMP_MISC_INFO_4>(
                  string_file.string(), directory[1].Location));

  EXPECT_EQ(kMinidumpStreamTypeThreadList, directory[2].StreamType);
  EXPECT_TRUE(MinidumpWritableAtLocationDescriptor<MINIDUMP_THREAD_LIST>(
                  string_file.string(), directory[2].Location));

  EXPECT_EQ(kMinidumpStreamTypeModuleList, directory[3].StreamType);
  EXPECT_TRUE(MinidumpWritableAtLocationDescriptor<MINIDUMP_MODULE_LIST>(
                  string_file.string(), directory[3].Location));

  EXPECT_EQ(kMinidumpStreamTypeMemoryList, directory[4].StreamType);
  EXPECT_TRUE(MinidumpWritableAtLocationDescriptor<MINIDUMP_MEMORY_LIST>(
                  string_file.string(), directory[4].Location));

  const MINIDUMP_MEMORY_LIST* memory_list =
      MinidumpWritableAtLocationDescriptor<MINIDUMP_MEMORY_LIST>(
          string_file.string(), directory[4].Location);
  EXPECT_EQ(1u, memory_list->NumberOfMemoryRanges);
  EXPECT_EQ(kPebAddress, memory_list->MemoryRanges[0].StartOfMemoryRange);
  EXPECT_EQ(kPebSize, memory_list->MemoryRanges[0].Memory.DataSize);
}

TEST(MinidumpFileWriter, InitializeFromSnapshot_Exception) {
  // In a 32-bit environment, this will give a “timestamp out of range” warning,
  // but the test should complete without failure.
  const uint32_t kSnapshotTime = 0xfd469ab8;
  MSVC_SUPPRESS_WARNING(4309);  // Truncation of constant value.
  MSVC_SUPPRESS_WARNING(4838);  // Narrowing conversion.
  const timeval kSnapshotTimeval = { static_cast<time_t>(kSnapshotTime), 0 };

  TestProcessSnapshot process_snapshot;
  process_snapshot.SetSnapshotTime(kSnapshotTimeval);

  auto system_snapshot = base::WrapUnique(new TestSystemSnapshot());
  system_snapshot->SetCPUArchitecture(kCPUArchitectureX86_64);
  system_snapshot->SetOperatingSystem(SystemSnapshot::kOperatingSystemMacOSX);
  process_snapshot.SetSystem(std::move(system_snapshot));

  auto thread_snapshot = base::WrapUnique(new TestThreadSnapshot());
  InitializeCPUContextX86_64(thread_snapshot->MutableContext(), 5);
  process_snapshot.AddThread(std::move(thread_snapshot));

  auto exception_snapshot = base::WrapUnique(new TestExceptionSnapshot());
  InitializeCPUContextX86_64(exception_snapshot->MutableContext(), 11);
  process_snapshot.SetException(std::move(exception_snapshot));

  // The module does not have anything that needs to be represented in a
  // MinidumpModuleCrashpadInfo structure, so no such structure is expected to
  // be present, which will in turn suppress the addition of a
  // MinidumpCrashpadInfo stream.
  auto module_snapshot = base::WrapUnique(new TestModuleSnapshot());
  process_snapshot.AddModule(std::move(module_snapshot));

  MinidumpFileWriter minidump_file_writer;
  minidump_file_writer.InitializeFromSnapshot(&process_snapshot);

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  const MINIDUMP_DIRECTORY* directory;
  const MINIDUMP_HEADER* header =
      MinidumpHeaderAtStart(string_file.string(), &directory);
  ASSERT_NO_FATAL_FAILURE(VerifyMinidumpHeader(header, 6, kSnapshotTime));
  ASSERT_TRUE(directory);

  EXPECT_EQ(kMinidumpStreamTypeSystemInfo, directory[0].StreamType);
  EXPECT_TRUE(MinidumpWritableAtLocationDescriptor<MINIDUMP_SYSTEM_INFO>(
                  string_file.string(), directory[0].Location));

  EXPECT_EQ(kMinidumpStreamTypeMiscInfo, directory[1].StreamType);
  EXPECT_TRUE(MinidumpWritableAtLocationDescriptor<MINIDUMP_MISC_INFO_4>(
                  string_file.string(), directory[1].Location));

  EXPECT_EQ(kMinidumpStreamTypeThreadList, directory[2].StreamType);
  EXPECT_TRUE(MinidumpWritableAtLocationDescriptor<MINIDUMP_THREAD_LIST>(
                  string_file.string(), directory[2].Location));

  EXPECT_EQ(kMinidumpStreamTypeException, directory[3].StreamType);
  EXPECT_TRUE(MinidumpWritableAtLocationDescriptor<MINIDUMP_EXCEPTION_STREAM>(
                  string_file.string(), directory[3].Location));

  EXPECT_EQ(kMinidumpStreamTypeModuleList, directory[4].StreamType);
  EXPECT_TRUE(MinidumpWritableAtLocationDescriptor<MINIDUMP_MODULE_LIST>(
                  string_file.string(), directory[4].Location));

  EXPECT_EQ(kMinidumpStreamTypeMemoryList, directory[5].StreamType);
  EXPECT_TRUE(MinidumpWritableAtLocationDescriptor<MINIDUMP_MEMORY_LIST>(
                  string_file.string(), directory[5].Location));
}

TEST(MinidumpFileWriter, InitializeFromSnapshot_CrashpadInfo) {
  const uint32_t kSnapshotTime = 0x15393bd3;
  const timeval kSnapshotTimeval = { static_cast<time_t>(kSnapshotTime), 0 };

  TestProcessSnapshot process_snapshot;
  process_snapshot.SetSnapshotTime(kSnapshotTimeval);

  auto system_snapshot = base::WrapUnique(new TestSystemSnapshot());
  system_snapshot->SetCPUArchitecture(kCPUArchitectureX86_64);
  system_snapshot->SetOperatingSystem(SystemSnapshot::kOperatingSystemMacOSX);
  process_snapshot.SetSystem(std::move(system_snapshot));

  auto thread_snapshot = base::WrapUnique(new TestThreadSnapshot());
  InitializeCPUContextX86_64(thread_snapshot->MutableContext(), 5);
  process_snapshot.AddThread(std::move(thread_snapshot));

  auto exception_snapshot = base::WrapUnique(new TestExceptionSnapshot());
  InitializeCPUContextX86_64(exception_snapshot->MutableContext(), 11);
  process_snapshot.SetException(std::move(exception_snapshot));

  // The module needs an annotation for the MinidumpCrashpadInfo stream to be
  // considered useful and be included.
  auto module_snapshot = base::WrapUnique(new TestModuleSnapshot());
  std::vector<std::string> annotations_list(1, std::string("annotation"));
  module_snapshot->SetAnnotationsVector(annotations_list);
  process_snapshot.AddModule(std::move(module_snapshot));

  MinidumpFileWriter minidump_file_writer;
  minidump_file_writer.InitializeFromSnapshot(&process_snapshot);

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  const MINIDUMP_DIRECTORY* directory;
  const MINIDUMP_HEADER* header =
      MinidumpHeaderAtStart(string_file.string(), &directory);
  ASSERT_NO_FATAL_FAILURE(VerifyMinidumpHeader(header, 7, kSnapshotTime));
  ASSERT_TRUE(directory);

  EXPECT_EQ(kMinidumpStreamTypeSystemInfo, directory[0].StreamType);
  EXPECT_TRUE(MinidumpWritableAtLocationDescriptor<MINIDUMP_SYSTEM_INFO>(
                  string_file.string(), directory[0].Location));

  EXPECT_EQ(kMinidumpStreamTypeMiscInfo, directory[1].StreamType);
  EXPECT_TRUE(MinidumpWritableAtLocationDescriptor<MINIDUMP_MISC_INFO_4>(
                  string_file.string(), directory[1].Location));

  EXPECT_EQ(kMinidumpStreamTypeThreadList, directory[2].StreamType);
  EXPECT_TRUE(MinidumpWritableAtLocationDescriptor<MINIDUMP_THREAD_LIST>(
                  string_file.string(), directory[2].Location));

  EXPECT_EQ(kMinidumpStreamTypeException, directory[3].StreamType);
  EXPECT_TRUE(MinidumpWritableAtLocationDescriptor<MINIDUMP_EXCEPTION_STREAM>(
                  string_file.string(), directory[3].Location));

  EXPECT_EQ(kMinidumpStreamTypeModuleList, directory[4].StreamType);
  EXPECT_TRUE(MinidumpWritableAtLocationDescriptor<MINIDUMP_MODULE_LIST>(
                  string_file.string(), directory[4].Location));

  EXPECT_EQ(kMinidumpStreamTypeCrashpadInfo, directory[5].StreamType);
  EXPECT_TRUE(MinidumpWritableAtLocationDescriptor<MinidumpCrashpadInfo>(
                  string_file.string(), directory[5].Location));

  EXPECT_EQ(kMinidumpStreamTypeMemoryList, directory[6].StreamType);
  EXPECT_TRUE(MinidumpWritableAtLocationDescriptor<MINIDUMP_MEMORY_LIST>(
                  string_file.string(), directory[6].Location));
}

TEST(MinidumpFileWriterDeathTest, SameStreamType) {
  MinidumpFileWriter minidump_file;

  const size_t kStream0Size = 5;
  const MinidumpStreamType kStream0Type = static_cast<MinidumpStreamType>(0x4d);
  const uint8_t kStream0Value = 0x5a;
  auto stream0 = base::WrapUnique(
      new TestStream(kStream0Type, kStream0Size, kStream0Value));
  minidump_file.AddStream(std::move(stream0));

  // It is an error to add a second stream of the same type.
  const size_t kStream1Size = 3;
  const MinidumpStreamType kStream1Type = static_cast<MinidumpStreamType>(0x4d);
  const uint8_t kStream1Value = 0xa5;
  auto stream1 = base::WrapUnique(
      new TestStream(kStream1Type, kStream1Size, kStream1Value));
  ASSERT_DEATH_CHECK(minidump_file.AddStream(std::move(stream1)),
                     "already present");
}

}  // namespace
}  // namespace test
}  // namespace crashpad
