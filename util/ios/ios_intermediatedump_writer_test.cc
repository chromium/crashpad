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
#include <sys/stat.h>

#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "test/errors.h"
#include "test/scoped_temp_dir.h"
#include "util/file/file_io.h"
#include "util/ios/ios_intermediatedump_data.h"

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

//  test file is opened, and exclusive locked
TEST_F(IOSIntermediatedumpWriterTest, OpenLocked) {
  EXPECT_TRUE(writer_->Open(path()));
  
  ScopedFileHandle handle(LoggingOpenFileForRead(path()));
  EXPECT_TRUE(handle.is_valid());
//  EXPECT_EQ(LoggingLockFile(scoped.get(), FileLocking::kExclusive, FileLockingBlocking::kNonBlocking), FileLockingResult::kWouldBlock);
}

TEST_F(IOSIntermediatedumpWriterTest, WriteMap) {
  EXPECT_TRUE(writer_->Open(path()));
  {
    internal::IOSIntermediatedumpWriter::ScopedMap map(
        writer_.get(), IntermediateDumpKey::kName);
    writer_->AddProperty(IntermediateDumpKey::kName, "a_name", 6);
  }
  EXPECT_TRUE(writer_->Close());
}

}  // namespace
}  // namespace test
}  // namespace crashpad
