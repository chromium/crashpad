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

#ifndef CRASHPAD_UTIL_FILE_DIRECTORY_H_
#define CRASHPAD_UTIL_FILE_DIRECTORY_H_

#include "base/files/file_path.h"

namespace crashpad {

//! \brief Determines if a directory exists.
//!
//! \param[in] path The path to check.
//! \return `true` if the path exists and is a directory. Otherwise `false`.
bool IsDirectory(const base::FilePath& path);

//! \brief Creates a directory, logging a message on failure.
//!
//! \param[in] path The path to the directory to create.
//! \param[in] may_reuse Whether to fail if the directory exists.
//! \return `true` if the directory is successfully created or it already
//!     existed and \a may_reuse is `true`. Otherwise, `false`.
bool LoggingCreateDirectory(const base::FilePath& path, bool may_reuse);

//! \brief Removes a directory, logging a message on failure.
//!
//! \param[in] path The to the directory to remove.
//! \return `true` if the directory was removed. Otherwise, `false`.
bool LoggingRemoveDirectory(const base::FilePath& path);

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_FILE_DIRECTORY_H_
