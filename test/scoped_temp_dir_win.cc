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

#include "test/scoped_temp_dir.h"

#include <windows.h>

#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "util/misc/random_string.h"
#include "gtest/gtest.h"

namespace crashpad {
namespace test {

namespace {

base::FilePath GenerateCandidateName() {
  wchar_t temp_path[MAX_PATH + 1];
  DWORD path_len = GetTempPath(MAX_PATH, temp_path);
  PCHECK(path_len != 0) << "GetTempPath";
  base::FilePath system_temp_dir(temp_path);
  base::string16 new_dir_name = base::UTF8ToUTF16(base::StringPrintf(
        "crashpad.test.%d.%s", GetCurrentProcessId(), RandomString().c_str()));
  return system_temp_dir.Append(new_dir_name);
}

constexpr int kRetries = 50;

}  // namespace

void ScopedTempDir::Rename() {
  for (int count = 0; count < kRetries; ++count) {
    // Try to move to a new temporary directory with a randomly generated name.
    // If the one we try exists, retry with a new name until we reach some
    // limit.
    base::FilePath target_path = GenerateCandidateName();
    if (MoveFileEx(path_.value().c_str(), target_path.value().c_str(), 0)) {
      path_ = target_path;
      return;
    }
  }

  CHECK(false) << "Couldn't move to a new unique temp dir";
}

// static
base::FilePath ScopedTempDir::CreateTemporaryDirectory() {
  for (int count = 0; count < kRetries; ++count) {
    // Try to create a new temporary directory with random generated name. If
    // the one we generate exists, keep trying another path name until we reach
    // some limit.
    base::FilePath path_to_create = GenerateCandidateName();
    if (CreateDirectory(path_to_create.value().c_str(), NULL))
      return path_to_create;
  }

  CHECK(false) << "Couldn't create a new unique temp dir";
  return base::FilePath();
}

// static
void ScopedTempDir::RecursivelyDeleteTemporaryDirectory(
    const base::FilePath& path) {
  const base::string16 all_files_mask(L"\\*");

  base::string16 search_mask = path.value() + all_files_mask;
  WIN32_FIND_DATA find_data;
  HANDLE search_handle = FindFirstFile(search_mask.c_str(), &find_data);
  if (search_handle == INVALID_HANDLE_VALUE)
    ASSERT_EQ(GetLastError(), ERROR_FILE_NOT_FOUND);
  do {
    if (wcscmp(find_data.cFileName, L".") == 0 ||
        wcscmp(find_data.cFileName, L"..") == 0) {
      continue;
    }
    base::FilePath entry_path = path.Append(find_data.cFileName);
    ASSERT_FALSE(find_data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT);
    if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
      RecursivelyDeleteTemporaryDirectory(entry_path);
    else
      EXPECT_TRUE(DeleteFile(entry_path.value().c_str()));
  } while (FindNextFile(search_handle, &find_data));
  EXPECT_EQ(GetLastError(), ERROR_NO_MORE_FILES);

  EXPECT_TRUE(FindClose(search_handle));
  EXPECT_TRUE(RemoveDirectory(path.value().c_str()));
}

}  // namespace test
}  // namespace crashpad
