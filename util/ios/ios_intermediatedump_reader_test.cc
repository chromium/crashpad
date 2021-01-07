// Copyright 2020 The Crashpad Authors. All rights reserved.
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
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/errors.h"
#include "test/scoped_temp_dir.h"
#include "util/ios/ios_intermediatedump_data.h"
#include "util/ios/ios_intermediatedump_writer.h"

namespace crashpad {
namespace test {
namespace {

class IOSIntermediatedumpReader : public testing::Test {
 protected:
  // testing::Test:

  void SetUp() override {
    path_ = temp_dir_.path().Append("dump_file");
    fd_ = base::ScopedFD(HANDLE_EINTR(
        ::open(path_.value().c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644)));
    EXPECT_GE(fd_.get(), 0) << ErrnoMessage("open");
  }

  void TearDown() override {
    EXPECT_EQ(unlink(path_.value().c_str()), 0) << ErrnoMessage("unlink");
  }

  void close() { fd_.reset(); }

  void write(std::string str) { ::write(fd_.get(), str.c_str(), str.length()); }

  int fd() { return fd_.get(); }

  const base::FilePath& path() const { return path_; }

 private:
  base::ScopedFD fd_;
  ScopedTempDir temp_dir_;
  base::FilePath path_;
};

TEST_F(IOSIntermediatedumpReader, ReadNoFile) {
  internal::IOSIntermediatedumpReader reader;
  EXPECT_FALSE(reader.Initialize(base::FilePath()));
}

TEST_F(IOSIntermediatedumpReader, ReadEmptyFile) {
  internal::IOSIntermediatedumpReader reader;
  EXPECT_FALSE(reader.Initialize(path()));
}

TEST_F(IOSIntermediatedumpReader, ReadHelloWorld) {
  write("hello world.");
  internal::IOSIntermediatedumpReader reader;
  EXPECT_TRUE(reader.Initialize(path()));
  uint8_t version = 0;
  EXPECT_FALSE(reader.RootMap()["version"].AsData().GetData<uint8_t>(&version));
  EXPECT_EQ(version, 0);
}

TEST_F(IOSIntermediatedumpReader, ReadValidData) {
  internal::IOSIntermediatedumpReader reader;
  uint8_t version = 1;
  internal::IOSIntermediatedumpWriter::Property(
      fd(), "version", &version, sizeof(version));
  EXPECT_TRUE(reader.Initialize(path()));
  version = 0;
  EXPECT_TRUE(reader.RootMap()["version"].AsData().GetData<uint8_t>(&version));
  EXPECT_EQ(version, 1);
}

TEST_F(IOSIntermediatedumpReader, WriteBadPropertyNameLength) {
  internal::IOSIntermediatedumpReader reader;
  uint8_t version = 1;
  uint8_t t = internal::PROPERTY;
  ::write(fd(), &t, sizeof(t));
  constexpr char name[] = "version";
  size_t name_length = 9999;
  ::write(fd(), &name_length, sizeof(size_t));
  ::write(fd(), name, strlen(name));
  uint8_t value = 1;
  size_t value_length = sizeof(value);
  ::write(fd(), &value_length, sizeof(size_t));
  ::write(fd(), &value, value_length);
  EXPECT_TRUE(reader.Initialize(path()));
  version = 0;
  EXPECT_FALSE(reader.RootMap()["version"].AsData().GetData<uint8_t>(&version));
  EXPECT_EQ(version, 0);
}

TEST_F(IOSIntermediatedumpReader, WriteBadPropertyDataLength) {
  internal::IOSIntermediatedumpReader reader;
  uint8_t version = 1;
  uint8_t t = internal::PROPERTY;
  ::write(fd(), &t, sizeof(t));
  constexpr char name[] = "version";
  size_t name_length = strlen(name);
  ::write(fd(), &name_length, sizeof(size_t));
  ::write(fd(), name, name_length);
  uint8_t value = 1;
  size_t value_length = 999999;
  ::write(fd(), &value_length, sizeof(size_t));
  ::write(fd(), &value, sizeof(value));
  EXPECT_TRUE(reader.Initialize(path()));
  version = 0;
  EXPECT_FALSE(reader.RootMap()["version"].AsData().GetData<uint8_t>(&version));
  EXPECT_EQ(version, 0);
}

TEST_F(IOSIntermediatedumpReader, InvalidArrays) {
  internal::IOSIntermediatedumpReader reader;

  uint8_t version = 1;
  internal::IOSIntermediatedumpWriter::IOSIntermediatedumpWriter::ArrayStart(
      fd(), "array1");
  internal::IOSIntermediatedumpWriter::IOSIntermediatedumpWriter::ArrayStart(
      fd(), "array2");
  // Write version last, so it's not parsed.
  internal::IOSIntermediatedumpWriter::Property(
      fd(), "version", &version, sizeof(version));
  EXPECT_TRUE(reader.Initialize(path()));
  version = 0;
  EXPECT_FALSE(reader.RootMap()["version"].AsData().GetData<uint8_t>(&version));
  EXPECT_EQ(version, 0);
}

TEST_F(IOSIntermediatedumpReader, InvalidPropertyInArray) {
  internal::IOSIntermediatedumpReader reader;

  uint8_t version = 1;
  internal::IOSIntermediatedumpWriter::IOSIntermediatedumpWriter::ArrayStart(
      fd(), "array1");
  internal::IOSIntermediatedumpWriter::Property(
      fd(), "property1", &version, sizeof(version));
  // Write version last, so it's not parsed.
  internal::IOSIntermediatedumpWriter::Property(
      fd(), "version", &version, sizeof(version));
  EXPECT_TRUE(reader.Initialize(path()));
  version = 0;
  EXPECT_FALSE(reader.RootMap()["version"].AsData().GetData<uint8_t>(&version));
  EXPECT_EQ(version, 0);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
