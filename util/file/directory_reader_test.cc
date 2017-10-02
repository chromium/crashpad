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

#include <vector>

#include "base/files/file_path.h"
#include "gtest/gtest.h"
#include "test/scoped_temp_dir.h"

namespace crashpad {
namespace test {
namespace {

TEST(DirectoryReader, BadDirectory) {
  {
    DirectoryReader reader;
    EXPECT_FALSE(reader.Open(base::FilePath()));
  }

  {
    DirectoryReader reader;
    EXPECT_FALSE(
        reader.Open(base::FilePath(FILE_PATH_LITERAL("notadirectory"))));
  }
}

TEST(DirectoryReader, Empty) {
  ScopedTempDir temp_dir;
  DirectoryReader reader;
  ASSERT_TRUE(reader.Open(temp_dir.path()));

  std::vector<base::FilePath> files;
  base::FilePath path;
  DirectoryReader::Result result;
  while ((result = reader.NextFile(&path)) ==
         DirectoryReader::Result::kFileFound) {
    files.push_back(path);
  }
  EXPECT_EQ(result, DirectoryReader::Result::kFileNotFound);
  EXPECT_EQ(files.size(), 2u);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
