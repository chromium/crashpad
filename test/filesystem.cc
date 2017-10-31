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

#include "test/filesystem.h"

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "gtest/gtest.h"
#include "test/errors.h"
#include "test/scoped_temp_dir.h"
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

namespace {

#if defined(OS_WIN)

// Detects the flags necessary to create symbolic links and stores them in
// |flags| if non-nullptr, and returns true on success. If symbolic links can’t
// be created, returns false.
bool SymbolicLinkFlags(DWORD* flags) {
  static DWORD symbolic_link_flags = []() {
    ScopedTempDir temp_dir_;

    base::FilePath target_path = temp_dir_.path().Append(L"target");
    base::FilePath symlink_path = temp_dir_.path().Append(L"symlink");
    if (::CreateSymbolicLink(
            symlink_path.value().c_str(), target_path.value().c_str(), 0)) {
      return 0;
    }

    DWORD error = GetLastError();
    if (error == ERROR_PRIVILEGE_NOT_HELD) {
      if (::CreateSymbolicLink(symlink_path.value().c_str(),
                               target_path.value().c_str(),
                               SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE)) {
        return SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
      }

      // This may fail with ERROR_INVALID_PARAMETER if the OS is too old to
      // understand SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE, so keep
      // ERROR_PRIVILEGE_NOT_HELD for |error|.
    }

    // Don’t use ErrorMessage() here because the second CreateSymbolicLink() may
    // have scrambled it. Use the saved |error| value instead.
    EXPECT_EQ(error, static_cast<DWORD>(ERROR_PRIVILEGE_NOT_HELD))
        << "CreateSymbolicLink: " << logging::SystemErrorCodeToString(error);
    return -1;
  }();

  if (symbolic_link_flags == static_cast<DWORD>(-1)) {
    return false;
  }

  if (flags) {
    *flags = symbolic_link_flags;
  }

  return true;
}

#endif  // OS_WIN

}  // namespace

bool CreateFile(const base::FilePath& file) {
  ScopedFileHandle fd(LoggingOpenFileForWrite(
      file, FileWriteMode::kCreateOrFail, FilePermissions::kOwnerOnly));
  EXPECT_TRUE(fd.is_valid());
  return fd.is_valid();
}

bool PathExists(const base::FilePath& path) {
#if defined(OS_POSIX)
  struct stat st;
  if (lstat(path.value().c_str(), &st) != 0) {
    EXPECT_EQ(errno, ENOENT) << ErrnoMessage("lstat ") << path.value();
    return false;
  }
  return true;
#elif defined(OS_WIN)
  if (GetFileAttributes(path.value().c_str()) == INVALID_FILE_ATTRIBUTES) {
    EXPECT_EQ(GetLastError(), static_cast<DWORD>(ERROR_FILE_NOT_FOUND))
        << ErrorMessage("GetFileAttributes ")
        << base::UTF16ToUTF8(path.value());
    return false;
  }
  return true;
#endif
}

#if !defined(OS_FUCHSIA)

bool CanCreateSymbolicLinks() {
#if defined(OS_POSIX)
  return true;
#elif defined(OS_WIN)
  return SymbolicLinkFlags(nullptr);
#endif  // OS_POSIX
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
  return true;
#elif defined(OS_WIN)
  DWORD symbolic_link_flags = 0;
  SymbolicLinkFlags(&symbolic_link_flags);
  if (!::CreateSymbolicLink(
          symlink_path.value().c_str(),
          target_path.value().c_str(),
          symbolic_link_flags |
              (IsDirectory(target_path, true) ? SYMBOLIC_LINK_FLAG_DIRECTORY
                                              : 0))) {
    PLOG(ERROR) << "CreateSymbolicLink";
    return false;
  }
  return true;
#endif  // OS_POSIX
}

#endif  // !OS_FUCHSIA

}  // namespace test
}  // namespace crashpad
