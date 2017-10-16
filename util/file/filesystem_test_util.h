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

#ifndef CRASHPAD_UTIL_FILE_FILESYSTEM_TEST_UTIL_H_
#define CRASHPAD_UTIL_FILE_FILESYSTEM_TEST_UTIL_H_

#include "base/files/file_path.h"

namespace crashpad {
namespace test {

bool CreateFile(const base::FilePath& file);

bool CreateSymbolicLink(const base::FilePath& target_path,
                        const base::FilePath& symlink_path);

}  // namespace test
}  // namespace crashpad

#endif  // CRASHPAD_UTIL_FILE_FILESYSTEM_TEST_UTIL_H_
