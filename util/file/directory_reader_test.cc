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

#include "util/file/directory_reader.h"

#include <set>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "test/scoped_temp_dir.h"
#include "util/file/directory.h"
#include "util/file/file_io.h"

namespace crashpad {
namespace test {
namespace {

void ExpectFiles(const std::set<base::FilePath>& files,
                 const std::set<base::FilePath>& expected) {
  EXPECT_EQ(files.size(), expected.size());

  for (const auto& filename : expected) {
#if defined(OS_WIN)
    SCOPED_TRACE(base::StringPrintf("Filename: %s",
                                    base::UTF16ToUTF8(filename.value().c_str())));
#else
    SCOPED_TRACE(base::StringPrintf("FIlename: %s",
                                    filename.value().c_str()));
#endif
    EXPECT_NE(files.find(filename), files.end());
  }
}

bool CreateFile(const base::FilePath file) {
  ScopedFileHandle fd(LoggingOpenFileForWrite(
      file, FileWriteMode::kCreateOrFail, FilePermissions::kOwnerOnly));
  EXPECT_TRUE(fd.is_valid());
  return fd.is_valid();
}

TEST(DirectoryReader, BadPaths) {
  bool has_error;
  {
    DirectoryReader reader(base::FilePath(), &has_error);
    EXPECT_TRUE(has_error);
    EXPECT_EQ(reader.begin(), reader.end());
  }

  ScopedTempDir temp_dir;
  {
    base::FilePath file(temp_dir.path().Append(FILE_PATH_LITERAL("file")));
    ASSERT_TRUE(CreateFile(file));
    DirectoryReader reader(file, &has_error);
    EXPECT_TRUE(has_error);
    EXPECT_EQ(reader.begin(), reader.end());
  }

  {
    DirectoryReader reader(
        temp_dir.path().Append(FILE_PATH_LITERAL("doesntexist")), &has_error);
    EXPECT_TRUE(has_error);
    EXPECT_EQ(reader.begin(), reader.end());
  }
}

TEST(DirectoryReader, Empty) {
  ScopedTempDir temp_dir;

  bool has_error;
  DirectoryReader reader(temp_dir.path(), &has_error);
  EXPECT_FALSE(has_error);

  EXPECT_EQ(reader.begin(), reader.end());
  EXPECT_FALSE(has_error);

  EXPECT_EQ(reader.NextFile(), nullptr);
  EXPECT_FALSE(has_error);
}

TEST(DirectoryReader, FilesAndDirectories) {
  ScopedTempDir temp_dir;
  std::set<base::FilePath> expected_files;

  {
    base::FilePath file(FILE_PATH_LITERAL("file"));
    ASSERT_TRUE(CreateFile(temp_dir.path().Append(file)));
    expected_files.insert(file);
  }

  {
    base::FilePath directory(FILE_PATH_LITERAL("directory"));
    ASSERT_TRUE(LoggingCreateDirectory(temp_dir.path().Append(directory)));
    expected_files.insert(directory);

    base::FilePath file(FILE_PATH_LITERAL("nested_file"));
    ASSERT_TRUE(CreateFile(temp_dir.path().Append(directory).Append(file)));
  }

  std::set<base::FilePath> files;
  bool has_error;
  for (const auto& filename : DirectoryReader(temp_dir.path(), &has_error)) {
    EXPECT_TRUE(files.insert(filename).second);
  }
  EXPECT_FALSE(has_error);
  ExpectFiles(files, expected_files);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
