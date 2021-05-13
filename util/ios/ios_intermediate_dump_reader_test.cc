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

#include "util/ios/ios_intermediate_dump_reader.h"

#include <fcntl.h>
#include <mach/vm_map.h>

#include "base/posix/eintr_wrapper.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/errors.h"
#include "test/scoped_temp_dir.h"
#include "util/ios/ios_intermediate_dump_data.h"
#include "util/ios/ios_intermediate_dump_list.h"
#include "util/ios/ios_intermediate_dump_writer.h"

namespace crashpad {
namespace test {
namespace {

using internal::IntermediateDumpKey;
using internal::IOSIntermediateDumpWriter;

class IOSIntermediateDumpReaderTest : public testing::Test {
 protected:
  // testing::Test:

  void SetUp() override {
    path_ = temp_dir_.path().Append("dump_file");
    fd_ = base::ScopedFD(HANDLE_EINTR(
        ::open(path_.value().c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644)));
    ASSERT_GE(fd_.get(), 0) << ErrnoMessage("open");

    writer_ = std::make_unique<IOSIntermediateDumpWriter>();
    ASSERT_TRUE(writer_->Open(path_));
  }

  void TearDown() override {
    fd_.reset();
    writer_.reset();
    ASSERT_EQ(unlink(path_.value().c_str()), 0) << ErrnoMessage("unlink");
  }

  int fd() { return fd_.get(); }

  const base::FilePath& path() const { return path_; }

  std::unique_ptr<IOSIntermediateDumpWriter> writer_;

 private:
  base::ScopedFD fd_;
  ScopedTempDir temp_dir_;
  base::FilePath path_;
};

TEST_F(IOSIntermediateDumpReaderTest, ReadNoFile) {
  internal::IOSIntermediateDumpReader reader;
  EXPECT_FALSE(reader.Initialize(base::FilePath()));
}

TEST_F(IOSIntermediateDumpReaderTest, ReadEmptyFile) {
  internal::IOSIntermediateDumpReader reader;
  EXPECT_FALSE(reader.Initialize(path()));
}

TEST_F(IOSIntermediateDumpReaderTest, ReadHelloWorld) {
  std::string hello_world("hello world.");
  EXPECT_TRUE(
      LoggingWriteFile(fd(), hello_world.c_str(), hello_world.length()));
  internal::IOSIntermediateDumpReader reader;
  EXPECT_TRUE(reader.Initialize(path()));
  EXPECT_FALSE(reader.Parse());

  const auto root_map = reader.RootMap();
  EXPECT_FALSE(root_map->HasKey(IntermediateDumpKey::kVersion));
  const auto version_data = root_map->GetAsData(IntermediateDumpKey::kVersion);
  EXPECT_EQ(version_data, nullptr);
}

TEST_F(IOSIntermediateDumpReaderTest, WriteBadPropertyDataLength) {
  internal::IOSIntermediateDumpReader reader;
  uint8_t t = internal::IOSIntermediateDumpWriter::PROPERTY;
  EXPECT_TRUE(LoggingWriteFile(fd(), &t, sizeof(t)));
  IntermediateDumpKey key = IntermediateDumpKey::kVersion;
  EXPECT_TRUE(LoggingWriteFile(fd(), &key, sizeof(key)));
  uint8_t value = 1;
  size_t value_length = 999999;
  EXPECT_TRUE(LoggingWriteFile(fd(), &value_length, sizeof(size_t)));
  EXPECT_TRUE(LoggingWriteFile(fd(), &value, sizeof(value)));
  EXPECT_TRUE(reader.Initialize(path()));
  EXPECT_FALSE(reader.Parse());

  const auto root_map = reader.RootMap();
  EXPECT_FALSE(root_map->HasKey(IntermediateDumpKey::kVersion));
  const auto version_data = root_map->GetAsData(IntermediateDumpKey::kVersion);
  EXPECT_EQ(version_data, nullptr);
}

TEST_F(IOSIntermediateDumpReaderTest, InvalidArrayInArray) {
  internal::IOSIntermediateDumpReader reader;
  {
    IOSIntermediateDumpWriter::ScopedArray threadArray(
        writer_.get(), IntermediateDumpKey::kThreads);
    IOSIntermediateDumpWriter::ScopedArray innerThreadArray(
        writer_.get(), IntermediateDumpKey::kModules);

    // Write version last, so it's not parsed.
    int8_t version = 1;
    writer_->AddProperty(IntermediateDumpKey::kVersion, &version);
  }
  EXPECT_TRUE(writer_->Close());
  EXPECT_TRUE(reader.Initialize(path()));
  EXPECT_FALSE(reader.Parse());

  const auto root_map = reader.RootMap();
  EXPECT_FALSE(root_map->HasKey(IntermediateDumpKey::kVersion));
  const auto version_data = root_map->GetAsData(IntermediateDumpKey::kVersion);
  EXPECT_EQ(version_data, nullptr);
}

TEST_F(IOSIntermediateDumpReaderTest, InvalidPropertyInArray) {
  internal::IOSIntermediateDumpReader reader;

  {
    IOSIntermediateDumpWriter::ScopedArray threadArray(
        writer_.get(), IntermediateDumpKey::kThreads);

    // Write version last, so it's not parsed.
    int8_t version = 1;
    writer_->AddProperty(IntermediateDumpKey::kVersion, &version);
  }
  EXPECT_TRUE(writer_->Close());

  EXPECT_TRUE(reader.Initialize(path()));
  EXPECT_FALSE(reader.Parse());

  const auto root_map = reader.RootMap();
  EXPECT_FALSE(root_map->HasKey(IntermediateDumpKey::kVersion));
  const auto version_data = root_map->GetAsData(IntermediateDumpKey::kVersion);
  EXPECT_EQ(version_data, nullptr);
}

TEST_F(IOSIntermediateDumpReaderTest, ReadValidData) {
  internal::IOSIntermediateDumpReader reader;
  uint8_t version = 1;
  EXPECT_TRUE(writer_->AddProperty(IntermediateDumpKey::kVersion, &version));

  {
    IOSIntermediateDumpWriter::ScopedArray threadArray(
        writer_.get(), IntermediateDumpKey::kThreadContextMemoryRegions);
    IOSIntermediateDumpWriter::ScopedMap threadMap(writer_.get());

    std::string random_data("random_data");
    EXPECT_TRUE(writer_->AddProperty(
        IntermediateDumpKey::kThreadContextMemoryRegionAddress, &version));
    EXPECT_TRUE(writer_->AddProperty(
        IntermediateDumpKey::kThreadContextMemoryRegionData,
        random_data.c_str(),
        random_data.length()));
  }

  {
    IOSIntermediateDumpWriter::ScopedMap map(writer_.get(),
                                             IntermediateDumpKey::kProcessInfo);
    pid_t p_pid = getpid();
    EXPECT_TRUE(writer_->AddProperty(IntermediateDumpKey::kPID, &p_pid));
  }

  EXPECT_TRUE(writer_->Close());
  EXPECT_TRUE(reader.Initialize(path()));
  EXPECT_TRUE(reader.Parse());

  auto root_map = reader.RootMap();
  version = -1;
  EXPECT_TRUE(root_map->HasKey(IntermediateDumpKey::kVersion));
  const auto version_data = root_map->GetAsData(IntermediateDumpKey::kVersion);
  ASSERT_NE(version_data, nullptr);
  EXPECT_TRUE(version_data->GetValue<uint8_t>(&version));
  EXPECT_EQ(version, 1);

  EXPECT_TRUE(root_map->HasKey(IntermediateDumpKey::kProcessInfo));
  const auto process_info =
      root_map->GetAsMap(IntermediateDumpKey::kProcessInfo);
  ASSERT_NE(process_info, nullptr);
  EXPECT_TRUE(process_info->HasKey(IntermediateDumpKey::kPID));
  const auto pid_data = process_info->GetAsData(IntermediateDumpKey::kPID);
  ASSERT_NE(pid_data, nullptr);
  pid_t p_pid = -1;
  EXPECT_TRUE(pid_data->GetValue<pid_t>(&p_pid));
  ASSERT_EQ(p_pid, getpid());

  const auto thread_context_memory_regions =
      root_map->GetAsList(IntermediateDumpKey::kThreadContextMemoryRegions);
  EXPECT_EQ(thread_context_memory_regions->size(), 1UL);
  for (const auto& region : *thread_context_memory_regions) {
    const auto data =
        region->GetAsData(IntermediateDumpKey::kThreadContextMemoryRegionData);
    ASSERT_NE(data, nullptr);
    vm_size_t data_size = data->length();
    EXPECT_EQ(data_size, 11UL);

    // Load as string.
    EXPECT_EQ(data->GetString(), "random_data");

    // Load as bytes.
    const char* data_bytes = reinterpret_cast<const char*>(data->bytes());
    EXPECT_EQ(std::string(data_bytes, data_size), "random_data");
  }

  const auto system_info = root_map->GetAsMap(IntermediateDumpKey::kSystemInfo);
  EXPECT_EQ(system_info, nullptr);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
