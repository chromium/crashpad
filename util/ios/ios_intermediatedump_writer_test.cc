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

#include "util/ios/ios_intermediatedump_writer.h"

#include <fcntl.h>

#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/errors.h"
#include "test/scoped_temp_dir.h"
#include "util/file/file_io.h"

namespace crashpad {
namespace test {
namespace {

using internal::IntermediateDumpKey;

class IOSIntermediatedumpWriterTest : public testing::Test {
 protected:
  // testing::Test:

  void SetUp() override {
    path_ = temp_dir_.path().Append("dump_file");
    writer_ = std::make_unique<internal::IOSIntermediatedumpWriter>();
  }

  void TearDown() override {
    writer_.reset();
    EXPECT_EQ(unlink(path_.value().c_str()), 0) << ErrnoMessage("unlink");
  }

  const base::FilePath& path() const { return path_; }

  std::unique_ptr<internal::IOSIntermediatedumpWriter> writer_;

 private:
  ScopedTempDir temp_dir_;
  base::FilePath path_;
};

// Test file is locked.
TEST_F(IOSIntermediatedumpWriterTest, OpenLocked) {
  EXPECT_TRUE(writer_->Open(path()));

  ScopedFileHandle handle(LoggingOpenFileForRead(path()));
  EXPECT_TRUE(handle.is_valid());
  EXPECT_EQ(LoggingLockFile(handle.get(),
                            FileLocking::kExclusive,
                            FileLockingBlocking::kNonBlocking),
            FileLockingResult::kWouldBlock);
}

TEST_F(IOSIntermediatedumpWriterTest, Close) {
  EXPECT_TRUE(writer_->Open(path()));
  EXPECT_TRUE(writer_->Close());

  std::string contents;
  ASSERT_TRUE(LoggingReadEntireFile(path(), &contents));
  ASSERT_EQ(contents, "\6");
}

TEST_F(IOSIntermediatedumpWriterTest, ScopedArray) {
  EXPECT_TRUE(writer_->Open(path()));
  internal::IOSIntermediatedumpWriter::ScopedArray threadArray(
      writer_.get(), IntermediateDumpKey::kThreads);
  internal::IOSIntermediatedumpWriter::ScopedMap threadMap(writer_.get());

  std::string contents;
  ASSERT_TRUE(LoggingReadEntireFile(path(), &contents));
  std::string result("\x3p\x17\0\0\0\0\0\0\x1", 10);
  ASSERT_EQ(contents, result);
}

TEST_F(IOSIntermediatedumpWriterTest, ScopedMap) {
  EXPECT_TRUE(writer_->Open(path()));
  internal::IOSIntermediatedumpWriter::ScopedMap map(
      writer_.get(), IntermediateDumpKey::kMachException);

  std::string contents;
  ASSERT_TRUE(LoggingReadEntireFile(path(), &contents));
  std::string result("\1\xe8\U00000003\0\0\0\0\0\0", 9);
  ASSERT_EQ(contents, result);
}

TEST_F(IOSIntermediatedumpWriterTest, Property) {
  EXPECT_TRUE(writer_->Open(path()));
  EXPECT_TRUE(
      writer_->AddProperty(IntermediateDumpKey::kVersion, "version", 7));

  std::string contents;
  ASSERT_TRUE(LoggingReadEntireFile(path(), &contents));
  std::string result("\5\1\0\0\0\0\0\0\0\a\0\0\0\0\0\0\0version", 24);
  ASSERT_EQ(contents, result);
}

TEST_F(IOSIntermediatedumpWriterTest, BadProperty) {
  EXPECT_TRUE(writer_->Open(path()));
  ASSERT_FALSE(
      writer_->AddProperty(IntermediateDumpKey::kVersion, "version", -1));

  std::string contents;
  ASSERT_TRUE(LoggingReadEntireFile(path(), &contents));

  // path() is now invalid, as type, key and value were written, but the
  // value itself is not.
  std::string results("\5\1\0\0\0\0\0\0\0\xff\xff\xff\xff\xff\xff\xff\xff", 17);
  ASSERT_EQ(contents, results);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
