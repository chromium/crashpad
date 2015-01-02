// Copyright 2015 The Crashpad Authors. All rights reserved.
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

#include "util/test/scoped_temp_dir.h"

#include <dirent.h>
#include <string.h>
#include <unistd.h>

#include "base/logging.h"
#include "gtest/gtest.h"
#include "util/test/errors.h"

namespace crashpad {
namespace test {

// static
base::FilePath ScopedTempDir::CreateTemporaryDirectory() {
  char dir_tempalate[] = "/tmp/com.googlecode.crashpad.test.XXXXXX";
  PCHECK(mkdtemp(dir_tempalate)) << "mkdtemp " << dir_tempalate;
  return base::FilePath(dir_tempalate);
}

// static
void ScopedTempDir::RecursivelyDeleteTemporaryDirectory(
    const base::FilePath& path) {
  DIR* dir = opendir(path.value().c_str());
  ASSERT_TRUE(dir) << ErrnoMessage("opendir") << " " << path.value();

  dirent* entry;
  while ((entry = readdir(dir))) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    base::FilePath entry_path = path.Append(entry->d_name);
    if (entry->d_type == DT_DIR) {
      RecursivelyDeleteTemporaryDirectory(entry_path);
    } else {
      EXPECT_EQ(0, unlink(entry_path.value().c_str()))
          << ErrnoMessage("unlink") << " " << entry_path.value();
    }
  }

  EXPECT_EQ(0, closedir(dir))
      << ErrnoMessage("closedir") << " " << path.value();
  EXPECT_EQ(0, rmdir(path.value().c_str()))
      << ErrnoMessage("rmdir") << " " << path.value();
}

}  // namespace test
}  // namespace crashpad
