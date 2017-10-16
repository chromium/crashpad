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

#include "util/file/filesystem.h"

#include "base/logging.h"
#include "gtest/gtest.h"
#include "test/file.h"
#include "test/scoped_temp_dir.h"
#include "util/file/filesystem_test_util.h"

namespace crashpad {
namespace test {
namespace {

TEST(Filesystem, CreateDirectory) {
  ScopedTempDir temp_dir;

  base::FilePath dir(temp_dir.path().Append(FILE_PATH_LITERAL("dir")));
  EXPECT_FALSE(IsDirectory(dir, false));

  ASSERT_TRUE(
      LoggingCreateDirectory(dir, FilePermissions::kWorldReadable, false));
  EXPECT_TRUE(IsDirectory(dir, false));

  EXPECT_FALSE(
      LoggingCreateDirectory(dir, FilePermissions::kWorldReadable, false));

  base::FilePath file(dir.Append(FILE_PATH_LITERAL("file")));
  ASSERT_TRUE(CreateFile(file));

  EXPECT_TRUE(
      LoggingCreateDirectory(dir, FilePermissions::kWorldReadable, true));
  EXPECT_TRUE(IsRegularFile(file));
}

TEST(Filesystem, IsRegularFile) {
  EXPECT_FALSE(IsRegularFile(base::FilePath()));

  ScopedTempDir temp_dir;
  EXPECT_FALSE(IsRegularFile(temp_dir.path()));

  base::FilePath file(temp_dir.path().Append(FILE_PATH_LITERAL("file")));
  EXPECT_FALSE(IsRegularFile(file));

  ASSERT_TRUE(CreateFile(file));
  EXPECT_TRUE(IsRegularFile(file));

  base::FilePath link(temp_dir.path().Append(FILE_PATH_LITERAL("link")));
  ASSERT_TRUE(CreateSymbolicLink(file, link));
  EXPECT_FALSE(IsRegularFile(link));

  ASSERT_TRUE(LoggingRemoveFile(file));
  EXPECT_FALSE(IsRegularFile(link));

  base::FilePath dir_link(
      temp_dir.path().Append((FILE_PATH_LITERAL("dir_link"))));
  ASSERT_TRUE(CreateSymbolicLink(temp_dir.path(), dir_link));
  EXPECT_FALSE(IsRegularFile(dir_link));
}

TEST(Filesystem, IsDirectory) {
  EXPECT_FALSE(IsDirectory(base::FilePath(), false));
  EXPECT_FALSE(IsDirectory(base::FilePath(), true));

  ScopedTempDir temp_dir;
  EXPECT_TRUE(IsDirectory(temp_dir.path(), false));

  base::FilePath file(temp_dir.path().Append(FILE_PATH_LITERAL("file")));
  EXPECT_FALSE(IsDirectory(file, false));
  EXPECT_FALSE(IsDirectory(file, true));

  ASSERT_TRUE(CreateFile(file));
  EXPECT_FALSE(IsDirectory(file, false));
  EXPECT_FALSE(IsDirectory(file, true));

  base::FilePath link(temp_dir.path().Append(FILE_PATH_LITERAL("link")));
  ASSERT_TRUE(CreateSymbolicLink(file, link));
  EXPECT_FALSE(IsDirectory(link, false));
  EXPECT_FALSE(IsDirectory(link, true));

  ASSERT_TRUE(LoggingRemoveFile(file));
  EXPECT_FALSE(IsDirectory(link, false));
  EXPECT_FALSE(IsDirectory(link, true));

  base::FilePath dir_link(
      temp_dir.path().Append(FILE_PATH_LITERAL("dir_link")));
  ASSERT_TRUE(CreateSymbolicLink(temp_dir.path(), dir_link));
  EXPECT_FALSE(IsDirectory(dir_link, false));
  EXPECT_TRUE(IsDirectory(dir_link, true));
}

TEST(Filesystem, RemoveFile) {
  EXPECT_FALSE(LoggingRemoveFile(base::FilePath()));

  ScopedTempDir temp_dir;
  EXPECT_FALSE(LoggingRemoveFile(temp_dir.path()));

  base::FilePath file(temp_dir.path().Append(FILE_PATH_LITERAL("file")));
  EXPECT_FALSE(LoggingRemoveFile(file));

  ASSERT_TRUE(CreateFile(file));
  EXPECT_TRUE(IsRegularFile(file));

  base::FilePath link(temp_dir.path().Append(FILE_PATH_LITERAL("link")));
  ASSERT_TRUE(CreateSymbolicLink(file, link));
  EXPECT_TRUE(LoggingRemoveFile(link));
  EXPECT_FALSE(FileExists(link));
  EXPECT_TRUE(FileExists(file));

  base::FilePath dir(temp_dir.path().Append(FILE_PATH_LITERAL("dir")));
  ASSERT_TRUE(
      LoggingCreateDirectory(dir, FilePermissions::kWorldReadable, false));
  EXPECT_FALSE(LoggingRemoveFile(dir));

  ASSERT_TRUE(CreateSymbolicLink(dir, link));
  EXPECT_TRUE(LoggingRemoveFile(link));
  EXPECT_FALSE(FileExists(link));
  EXPECT_TRUE(FileExists(dir));

  EXPECT_TRUE(LoggingRemoveFile(file));
  EXPECT_FALSE(IsRegularFile(file));
  EXPECT_FALSE(LoggingRemoveFile(file));
}

TEST(Filesystem, RemoveDirectory) {
  EXPECT_FALSE(LoggingRemoveDirectory(base::FilePath()));

  ScopedTempDir temp_dir;

  base::FilePath dir(temp_dir.path().Append(FILE_PATH_LITERAL("dir")));
  ASSERT_TRUE(
      LoggingCreateDirectory(dir, FilePermissions::kWorldReadable, false));

  base::FilePath file(dir.Append(FILE_PATH_LITERAL("file")));
  EXPECT_FALSE(LoggingRemoveDirectory(file));

  ASSERT_TRUE(CreateFile(file));
  EXPECT_FALSE(LoggingRemoveDirectory(file));
  EXPECT_FALSE(LoggingRemoveDirectory(dir));

  base::FilePath link(temp_dir.path().Append(FILE_PATH_LITERAL("link")));
  ASSERT_TRUE(CreateSymbolicLink(file, link));
  EXPECT_FALSE(LoggingRemoveDirectory(link));
  EXPECT_TRUE(LoggingRemoveFile(link));

  ASSERT_TRUE(CreateSymbolicLink(dir, link));
  EXPECT_FALSE(LoggingRemoveDirectory(link));
  EXPECT_TRUE(LoggingRemoveFile(link));

  ASSERT_TRUE(LoggingRemoveFile(file));
  EXPECT_TRUE(LoggingRemoveDirectory(dir));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
