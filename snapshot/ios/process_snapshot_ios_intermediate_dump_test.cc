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
#include "gtest/gtest.h"
#include "minidump/minidump_file_writer.h"
#include "test/errors.h"
#include "test/scoped_temp_dir.h"
#include "util/file/file_io.h"
#include "util/file/string_file.h"

namespace crashpad {
namespace test {
namespace {

using Key = internal::IntermediateDumpKey;
using internal::IOSIntermediateDumpWriter;

class ProcessSnapshotIOSIntermediateDumpTest : public testing::Test {
 protected:
  // testing::Test:

  void SetUp() override {
    path_ = temp_dir_.path().Append("dump_file");
    writer_ = std::make_unique<internal::IOSIntermediateDumpWriter>();
    EXPECT_TRUE(writer_->Open(path_));
  }

  void TearDown() override {
    writer_.reset();
    EXPECT_EQ(unlink(path_.value().c_str()), 0) << ErrnoMessage("unlink");
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
}

TEST_F(ProcessSnapshotIOSIntermediateDumpTest, InitializeEmpty) {
  ProcessSnapshotIOSIntermediateDump process_snapshot;
  EXPECT_FALSE(process_snapshot.Initialize(path(), annotations()));
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
}

TEST_F(ProcessSnapshotIOSIntermediateDumpTest, EmptySignalDump) {
  {
    IOSIntermediateDumpWriter::ScopedRootMap rootMap(writer());
    uint8_t version = 1;
    EXPECT_TRUE(writer()->AddProperty(Key::kVersion, &version));
    { IOSIntermediateDumpWriter::ScopedMap map(writer(), Key::kSystemInfo); }
    { IOSIntermediateDumpWriter::ScopedMap map(writer(), Key::kProcessInfo); }
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
  EXPECT_TRUE(DumpSnapshot(process_snapshot));
}

TEST_F(ProcessSnapshotIOSIntermediateDumpTest, EmptyMachDump) {
  {
    IOSIntermediateDumpWriter::ScopedRootMap rootMap(writer());
    uint8_t version = 1;
    EXPECT_TRUE(writer()->AddProperty(Key::kVersion, &version));
    { IOSIntermediateDumpWriter::ScopedMap map(writer(), Key::kSystemInfo); }
    { IOSIntermediateDumpWriter::ScopedMap map(writer(), Key::kProcessInfo); }
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
  EXPECT_TRUE(DumpSnapshot(process_snapshot));
}

TEST_F(ProcessSnapshotIOSIntermediateDumpTest, EmptyExceptionDump) {
  {
    IOSIntermediateDumpWriter::ScopedRootMap rootMap(writer());
    uint8_t version = 1;
    EXPECT_TRUE(writer()->AddProperty(Key::kVersion, &version));
    { IOSIntermediateDumpWriter::ScopedMap map(writer(), Key::kSystemInfo); }
    { IOSIntermediateDumpWriter::ScopedMap map(writer(), Key::kProcessInfo); }
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
  EXPECT_TRUE(DumpSnapshot(process_snapshot));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
