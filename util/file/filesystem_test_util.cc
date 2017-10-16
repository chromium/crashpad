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

#include "util/file/filesystem_test_util.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "util/file/file_io.h"
#include "util/file/filesystem.h"

#if defined(OS_POSIX)
#include <unistd.h>

#include "base/posix/eintr_wrapper.h"
#elif defined(OS_WIN)
#include <windows.h>
#endif

namespace crashpad {
namespace test {

bool CreateFile(const base::FilePath& file) {
  ScopedFileHandle fd(LoggingOpenFileForWrite(
      file, FileWriteMode::kCreateOrFail, FilePermissions::kOwnerOnly));
  EXPECT_TRUE(fd.is_valid());
  return fd.is_valid();
}

bool CreateSymbolicLink(const base::FilePath& target_path,
                        const base::FilePath& symlink_path) {
#if defined(OS_POSIX)
  int rv = HANDLE_EINTR(
      symlink(target_path.value().c_str(), symlink_path.value().c_str()));
  if (rv != 0) {
    PLOG(ERROR) << "symlink";
    return false;
  }
#elif defined(OS_WIN)
  if (!::CreateSymbolicLink(
          symlink_path.value().c_str(),
          target_path.value().c_str(),
          IsDirectory(target_path, true) ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0)) {
    PLOG(ERROR) << "CreateSymbolicLink";
    return false;
  }
#endif  // OS_POSIX
  return true;
}

}  // namespace test
}  // namespace crashpad
