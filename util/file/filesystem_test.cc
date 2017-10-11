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

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "test/scoped_temp_dir.h"
#include "util/file/file_io.h"

#if defined(OS_POSIX)
#include <unistd.h>
#elif defined(OS_WIN)
#include <windows.h>
#endif

namespace crashpad {
namespace test {
namespace {

bool CreateFile(const base::FilePath file) {
  ScopedFileHandle fd(LoggingOpenFileForWrite(
      file, FileWriteMode::kCreateOrFail, FilePermissions::kOwnerOnly));
  EXPECT_TRUE(fd.is_valid());
  return fd.is_valid();
}

bool CreateLink(const base::FilePath source, const base::FilePath dest) {
#if defined(OS_POSIX)
  int rv = symlink(source.value().c_str(), dest.value().c_str());
  if (rv != 0) {
    PLOG(ERROR) << "symlink";
    return false;
  }
#elif defined(OS_WIN)
  if (!CreateSymbolicLink(dest.value().c_str(),
                          source.value().c_str(),
                          IsDirectory(source, true) ?
                            SYMBOLIC_LINK_FLAG_DIRECTORY) :
                            0) {
    PLOG(ERROR) << "CreateSymbolicLink";
    return false;
  }
#endif  // OS_POSIX
  return true;
}

TEST(Filesystem, CreateDirectory) {
  ScopedTempDir temp_dir;

  base::FilePath dir(temp_dir.path().Append(FILE_PATH_LITERAL("dir")));
  EXPECT_FALSE(IsDirectory(dir, false));

  ASSERT_TRUE(LoggingCreateDirectory(dir, false));
  EXPECT_TRUE(IsDirectory(dir, false));

  EXPECT_FALSE(LoggingCreateDirectory(dir, false));
  EXPECT_TRUE(LoggingCreateDirectory(dir, true));
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
  ASSERT_TRUE(CreateLink(file, link));
  EXPECT_FALSE(IsRegularFile(link));
}

TEST(Filesystem, IsDirectory) {
  EXPECT_FALSE(IsDirectory(base::FilePath(), false));
  EXPECT_FALSE(IsDirectory(base::FilePath(), true));

  ScopedTempDir temp_dir;
  EXPECT_TRUE(IsDirectory(temp_dir.path(), false));

  base::FilePath file(temp_dir.path().Append(FILE_PATH_LITERAL("file")));
  EXPECT_FALSE(IsDirectory(file, false));
  EXPECT_FALSE(IsDirectory(file, true));

  base::FilePath link(temp_dir.path().Append(FILE_PATH_LITERAL("link")));
  ASSERT_TRUE(CreateLink(temp_dir.path(), link));
  EXPECT_FALSE(IsDirectory(link, false));
  EXPECT_TRUE(IsDirectory(link, true));
}

TEST(Filesystem, DeleteFile) {
  EXPECT_FALSE(LoggingDeleteFile(base::FilePath()));

  ScopedTempDir temp_dir;
  EXPECT_FALSE(LoggingDeleteFile(temp_dir.path()));

  base::FilePath file(temp_dir.path().Append(FILE_PATH_LITERAL("file")));
  EXPECT_FALSE(LoggingDeleteFile(file));

  ASSERT_TRUE(CreateFile(file));
  EXPECT_TRUE(IsRegularFile(file));
  EXPECT_TRUE(LoggingDeleteFile(file));
  EXPECT_FALSE(IsRegularFile(file));
}

TEST(Filesystem, RemoveDirectory) {
  EXPECT_FALSE(LoggingRemoveDirectory(base::FilePath()));

  ScopedTempDir temp_dir;

  base::FilePath dir(temp_dir.path().Append(FILE_PATH_LITERAL("dir")));
  ASSERT_TRUE(LoggingCreateDirectory(dir, false));

  base::FilePath file(dir.Append(FILE_PATH_LITERAL("file")));
  EXPECT_FALSE(LoggingRemoveDirectory(file));

  ASSERT_TRUE(CreateFile(file));
  EXPECT_FALSE(LoggingRemoveDirectory(file));
  EXPECT_FALSE(LoggingRemoveDirectory(dir));

  ASSERT_TRUE(LoggingDeleteFile(file));
  EXPECT_TRUE(LoggingRemoveDirectory(dir));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
