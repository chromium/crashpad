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

#include "util/ios/ios_intermediatedump_reader.h"

#include <fcntl.h>
#include <sys/stat.h>

#include "base/posix/eintr_wrapper.h"
#include "base/scoped_generic.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/errors.h"
#include "test/scoped_temp_dir.h"
#include "util/ios/ios_intermediatedump_data.h"
#include "util/ios/ios_intermediatedump_list.h"
#include "util/ios/ios_intermediatedump_writer.h"

namespace crashpad {
namespace test {
namespace {

using internal::IntermediateDumpKey;
using internal::IOSIntermediatedumpWriter;

class IOSIntermediatedumpReaderTest : public testing::Test {
 protected:
  // testing::Test:

  void SetUp() override {
    path_ = temp_dir_.path().Append("dump_file");
    fd_ = base::ScopedFD(HANDLE_EINTR(
        ::open(path_.value().c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644)));
    EXPECT_GE(fd_.get(), 0) << ErrnoMessage("open");

    writer_ = std::make_unique<IOSIntermediatedumpWriter>();
    EXPECT_TRUE(writer_->Open(path_));
  }

  void TearDown() override {
    fd_.reset();
    writer_.reset();
    EXPECT_EQ(unlink(path_.value().c_str()), 0) << ErrnoMessage("unlink");
  }

  int fd() { return fd_.get(); }

  const base::FilePath& path() const { return path_; }

  std::unique_ptr<IOSIntermediatedumpWriter> writer_;

 private:
  base::ScopedFD fd_;
  ScopedTempDir temp_dir_;
  base::FilePath path_;
};

TEST_F(IOSIntermediatedumpReaderTest, ReadNoFile) {
  internal::IOSIntermediatedumpReader reader;
  EXPECT_FALSE(reader.Initialize(base::FilePath()));
}

TEST_F(IOSIntermediatedumpReaderTest, ReadEmptyFile) {
  internal::IOSIntermediatedumpReader reader;
  EXPECT_FALSE(reader.Initialize(path()));
}

TEST_F(IOSIntermediatedumpReaderTest, ReadHelloWorld) {
  std::string hello_world("hello world.");
  EXPECT_TRUE(
      LoggingWriteFile(fd(), hello_world.c_str(), hello_world.length()));
  internal::IOSIntermediatedumpReader reader;
  EXPECT_TRUE(reader.Initialize(path()));
  EXPECT_FALSE(reader.Parse());
  uint8_t version = 0;
  EXPECT_FALSE(
      reader.RootMap()[IntermediateDumpKey::kVersion].AsData().GetData<uint8_t>(
          &version));
  EXPECT_EQ(version, 0);
}

TEST_F(IOSIntermediatedumpReaderTest, WriteBadPropertyDataLength) {
  internal::IOSIntermediatedumpReader reader;
  uint8_t version = 1;
  uint8_t t = internal::PROPERTY;
  EXPECT_TRUE(LoggingWriteFile(fd(), &t, sizeof(t)));
  IntermediateDumpKey key = IntermediateDumpKey::kVersion;
  EXPECT_TRUE(LoggingWriteFile(fd(), &key, sizeof(key)));
  uint8_t value = 1;
  size_t value_length = 999999;
  EXPECT_TRUE(LoggingWriteFile(fd(), &value_length, sizeof(size_t)));
  EXPECT_TRUE(LoggingWriteFile(fd(), &value, sizeof(value)));
  EXPECT_TRUE(reader.Initialize(path()));
  EXPECT_FALSE(reader.Parse());
  version = 0;
  EXPECT_FALSE(
      reader.RootMap()[IntermediateDumpKey::kVersion].AsData().GetData<uint8_t>(
          &version));
  EXPECT_EQ(version, 0);
}

TEST_F(IOSIntermediatedumpReaderTest, InvalidArrays) {
  internal::IOSIntermediatedumpReader reader;

  uint8_t version = 1;
  IOSIntermediatedumpWriter::ScopedArray threadArray(
      writer_.get(), IntermediateDumpKey::kThreads);
  IOSIntermediatedumpWriter::ScopedArray moduleArray(
      writer_.get(), IntermediateDumpKey::kModules);

  // Write version last, so it's not parsed.
  writer_->AddProperty(
      IntermediateDumpKey::kVersion, &version, sizeof(version));
  writer_->Close();
  EXPECT_TRUE(reader.Initialize(path()));
  EXPECT_FALSE(reader.Parse());
  version = 0;
  EXPECT_FALSE(
      reader.RootMap()[IntermediateDumpKey::kVersion].AsData().GetData<uint8_t>(
          &version));
  EXPECT_EQ(version, 0);
}

TEST_F(IOSIntermediatedumpReaderTest, InvalidPropertyInArray) {
  internal::IOSIntermediatedumpReader reader;

  uint8_t version = 1;
  IOSIntermediatedumpWriter::ScopedArray threadArray(
      writer_.get(), IntermediateDumpKey::kThreads);
  // Write version last, so it's not parsed.
  writer_->AddProperty(IntermediateDumpKey::kPID, &version, sizeof(version));
  // Write version last, so it's not parsed.
  writer_->AddProperty(
      IntermediateDumpKey::kVersion, &version, sizeof(version));
  EXPECT_TRUE(reader.Initialize(path()));
  EXPECT_FALSE(reader.Parse());
  version = 0;
  EXPECT_FALSE(
      reader.RootMap()[IntermediateDumpKey::kVersion].AsData().GetData<uint8_t>(
          &version));
  EXPECT_EQ(version, 0);
}

TEST_F(IOSIntermediatedumpReaderTest, ReadValidData) {
  internal::IOSIntermediatedumpReader reader;
  uint8_t version = 1;
  EXPECT_TRUE(writer_->AddProperty(
      IntermediateDumpKey::kVersion, &version, sizeof(version)));

  {
    IOSIntermediatedumpWriter::ScopedArray threadArray(
        writer_.get(), IntermediateDumpKey::kThreadContextMemoryRegions);
    IOSIntermediatedumpWriter::ScopedMap threadMap(writer_.get());

    std::string random_data("random_data");
    EXPECT_TRUE(writer_->AddProperty(
        IntermediateDumpKey::kThreadContextMemoryRegionAddress,
        &version,
        sizeof(version)));
    EXPECT_TRUE(writer_->AddProperty(
        IntermediateDumpKey::kThreadContextMemoryRegionData,
        random_data.c_str(),
        random_data.length()));
  }

  {
    IOSIntermediatedumpWriter::ScopedMap map(writer_.get(),
                                             IntermediateDumpKey::kProcessInfo);
    pid_t p_pid = getpid();
    EXPECT_TRUE(
        writer_->AddProperty(IntermediateDumpKey::kPID, &p_pid, sizeof(pid_t)));
  }

  EXPECT_TRUE(writer_->Close());
  EXPECT_TRUE(reader.Initialize(path()));
  EXPECT_TRUE(reader.Parse());

  auto& root_map = reader.RootMap();
  version = 0;
  EXPECT_TRUE(root_map[IntermediateDumpKey::kVersion].AsData().GetData<uint8_t>(
      &version));
  EXPECT_EQ(version, 1);

  const internal::IOSIntermediatedumpMap& process_info =
      root_map[IntermediateDumpKey::kProcessInfo].AsMap();
  pid_t p_pid = 0;
  EXPECT_TRUE(
      process_info[IntermediateDumpKey::kPID].AsData().GetData<pid_t>(&p_pid));
  ASSERT_EQ(p_pid, getpid());

  const auto& thread_context_memory_regions =
      root_map[IntermediateDumpKey::kThreadContextMemoryRegions].AsList();
  for (auto& region : thread_context_memory_regions) {
    const internal::IOSIntermediatedumpMap& region_map = (*region).AsMap();
    vm_size_t data_size =
        region_map[IntermediateDumpKey::kThreadContextMemoryRegionData]
            .AsData()
            .length();
    EXPECT_EQ(data_size, 11UL);

    // Load as string.
    std::string name;
    region_map[IntermediateDumpKey::kThreadContextMemoryRegionData].GetString(
        &name);
    EXPECT_EQ(name, "random_data");

    // Load as data.
    vm_address_t data =
        (vm_address_t)
            region_map[IntermediateDumpKey::kThreadContextMemoryRegionData]
                .AsData()
                .data();
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(data), data_size),
              "random_data");
  }

  // It's valid to check for data that doesn't exist.
  const internal::IOSIntermediatedumpMap& system_info =
      root_map[IntermediateDumpKey::kSystemInfo].AsMap();
  pid_t parent_pid = 0;
  EXPECT_FALSE(
      system_info[IntermediateDumpKey::kParentPID].AsData().GetData<pid_t>(
          &parent_pid));
  ASSERT_EQ(parent_pid, 0);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
