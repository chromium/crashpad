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

#ifndef CRASHPAD_TEST_FILESYSTEM_H_
#define CRASHPAD_TEST_FILESYSTEM_H_

#include "base/files/file_path.h"

#include "build/build_config.h"

namespace crashpad {
namespace test {

bool CreateFile(const base::FilePath& file);

bool PathExists(const base::FilePath& path);

#if !defined(OS_FUCHSIA) || DOXYGEN
// There are no symbolic links on Fuchsia. Don’t bother declaring or defining
// symbolic link-related functions at all, because it’s an error to even pretend
// that symbolic links might be available on Fuchsia.

//! \brief Determines whether it should be possible to create symbolic links.
//!
//! It is always possible to create symbolic links on POSIX.
//!
//! On Windows, it is only possible to create symbolic links when running as an
//! administrator, or as a non-administrator when running Windows 10 build 15063
//! (1703, Creators Update) or later, provided that developer mode is enabled
//! and `SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE` is used. This function
//! tests the creation of a symbolic link and returns true on success, and false
//! on failure. If the symbolic link could not be created for a reason other
//! than the expected lack of privilege, a message is logged.
//!
//! Additional background: <a
//! href="https://blogs.windows.com/buildingapps/2016/12/02/symlinks-windows-10/">Symlinks
//! in Windows 10!</a>
bool CanCreateSymbolicLinks();

bool CreateSymbolicLink(const base::FilePath& target_path,
                        const base::FilePath& symlink_path);

#endif  // !OS_FUCHSIA || DOXYGEN

}  // namespace test
}  // namespace crashpad

#endif  // CRASHPAD_TEST_FILESYSTEM_H_
