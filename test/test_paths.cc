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

#include "test/test_paths.h"

#include <stdlib.h>
#include <sys/stat.h>

#include "base/logging.h"
#include "build/build_config.h"
#include "util/misc/paths.h"

namespace crashpad {
namespace test {

namespace {

bool IsTestDataRoot(const base::FilePath& candidate) {
  const base::FilePath marker_path =
      candidate.Append(FILE_PATH_LITERAL("test"))
          .Append(FILE_PATH_LITERAL("test_paths_test_data_root.txt"));

#if !defined(OS_WIN)
  struct stat stat_buf;
  int rv = stat(marker_path.value().c_str(), &stat_buf);
#else
  struct _stat stat_buf;
  int rv = _wstat(marker_path.value().c_str(), &stat_buf);
#endif

  return rv == 0;
}

base::FilePath TestDataRootInternal() {
#if !defined(OS_WIN)
  const char* environment_value = getenv("CRASHPAD_TEST_DATA_ROOT");
#else  // defined(OS_WIN)
  const wchar_t* environment_value = _wgetenv(L"CRASHPAD_TEST_DATA_ROOT");
#endif

  if (environment_value) {
    // It was specified explicitly, so use it even if it seems incorrect.
    if (!IsTestDataRoot(base::FilePath(environment_value))) {
      LOG(WARNING) << "CRASHPAD_TEST_DATA_ROOT seems invalid, honoring anyway";
    }

    return base::FilePath(environment_value);
  }

  // In a standalone build, the test executable is usually at
  // out/{Debug,Release} relative to the Crashpad root.
  base::FilePath executable_path;
  if (Paths::Executable(&executable_path)) {
    base::FilePath candidate =
        base::FilePath(executable_path.DirName()
                           .Append(base::FilePath::kParentDirectory)
                           .Append(base::FilePath::kParentDirectory));
    if (IsTestDataRoot(candidate)) {
      return candidate;
    }

    // In an in-Chromium build, the test executable is usually at
    // out/{Debug,Release} relative to the Chromium root, and the Crashpad root
    // is at third_party/crashpad/crashpad relative to the Chromium root.
    candidate = candidate.Append(FILE_PATH_LITERAL("third_party"))
                    .Append(FILE_PATH_LITERAL("crashpad"))
                    .Append(FILE_PATH_LITERAL("crashpad"));
    if (IsTestDataRoot(candidate)) {
      return candidate;
    }
  }

  // If nothing else worked, use the current directory, issuing a warning if it
  // doesn’t seem right.
  if (!IsTestDataRoot(base::FilePath(base::FilePath::kCurrentDirectory))) {
    LOG(WARNING) << "could not locate a valid test data root";
  }

  return base::FilePath(base::FilePath::kCurrentDirectory);
}

}  // namespace

// static
base::FilePath TestPaths::Executable() {
  base::FilePath executable_path;
  CHECK(Paths::Executable(&executable_path));
  return executable_path;
}

// static
base::FilePath TestPaths::TestDataRoot() {
  static base::FilePath* test_data_root =
      new base::FilePath(TestDataRootInternal());
  return *test_data_root;
}

#if defined(OS_WIN) && defined(ARCH_CPU_64_BITS)

// static
base::FilePath TestPaths::Output32BitDirectory() {
  const wchar_t* environment_value = _wgetenv(L"CRASHPAD_TEST_32_BIT_OUTPUT");
  if (!environment_value) {
    return base::FilePath();
  }

  return base::FilePath(environment_value);
}

#endif  // defined(OS_WIN) && defined(ARCH_CPU_64_BITS)

}  // namespace test
}  // namespace crashpad
