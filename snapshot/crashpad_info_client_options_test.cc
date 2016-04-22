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

#include "snapshot/crashpad_info_client_options.h"

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "client/crashpad_info.h"
#include "gtest/gtest.h"
#include "test/errors.h"
#include "test/paths.h"

#if defined(OS_MACOSX)
#include <dlfcn.h>
#include "snapshot/mac/process_snapshot_mac.h"
#elif defined(OS_WIN)
#include <windows.h>
#include "snapshot/win/process_snapshot_win.h"
#endif

namespace crashpad {
namespace test {
namespace {

TEST(CrashpadInfoClientOptions, TriStateFromCrashpadInfo) {
  EXPECT_EQ(TriState::kUnset,
            CrashpadInfoClientOptions::TriStateFromCrashpadInfo(0));
  EXPECT_EQ(TriState::kEnabled,
            CrashpadInfoClientOptions::TriStateFromCrashpadInfo(1));
  EXPECT_EQ(TriState::kDisabled,
            CrashpadInfoClientOptions::TriStateFromCrashpadInfo(2));

  // These will produce log messages but should result in kUnset being returned.
  EXPECT_EQ(TriState::kUnset,
            CrashpadInfoClientOptions::TriStateFromCrashpadInfo(3));
  EXPECT_EQ(TriState::kUnset,
            CrashpadInfoClientOptions::TriStateFromCrashpadInfo(4));
  EXPECT_EQ(TriState::kUnset,
            CrashpadInfoClientOptions::TriStateFromCrashpadInfo(0xff));
}

class ScopedUnsetCrashpadInfoOptions {
 public:
  explicit ScopedUnsetCrashpadInfoOptions(CrashpadInfo* crashpad_info)
      : crashpad_info_(crashpad_info) {
  }

  ~ScopedUnsetCrashpadInfoOptions() {
    crashpad_info_->set_crashpad_handler_behavior(TriState::kUnset);
    crashpad_info_->set_system_crash_reporter_forwarding(TriState::kUnset);
    crashpad_info_->set_gather_indirectly_referenced_memory(TriState::kUnset,
                                                            0);
  }

 private:
  CrashpadInfo* crashpad_info_;

  DISALLOW_COPY_AND_ASSIGN(ScopedUnsetCrashpadInfoOptions);
};

CrashpadInfoClientOptions SelfProcessSnapshotAndGetCrashpadOptions() {
#if defined(OS_MACOSX)
  ProcessSnapshotMac process_snapshot;
  EXPECT_TRUE(process_snapshot.Initialize(mach_task_self()));
#elif defined(OS_WIN)
  ProcessSnapshotWin process_snapshot;
  EXPECT_TRUE(process_snapshot.Initialize(
      GetCurrentProcess(), ProcessSuspensionState::kRunning, 0));
#else
#error Port.
#endif  // OS_MACOSX

  CrashpadInfoClientOptions options;
  process_snapshot.GetCrashpadOptions(&options);
  return options;
}

TEST(CrashpadInfoClientOptions, OneModule) {
  // Make sure that the initial state has all values unset.
  auto options = SelfProcessSnapshotAndGetCrashpadOptions();

  EXPECT_EQ(TriState::kUnset, options.crashpad_handler_behavior);
  EXPECT_EQ(TriState::kUnset, options.system_crash_reporter_forwarding);
  EXPECT_EQ(TriState::kUnset, options.gather_indirectly_referenced_memory);
  EXPECT_EQ(0u, options.indirectly_referenced_memory_cap);

  CrashpadInfo* crashpad_info = CrashpadInfo::GetCrashpadInfo();
  ASSERT_TRUE(crashpad_info);

  {
    ScopedUnsetCrashpadInfoOptions unset(crashpad_info);

    crashpad_info->set_crashpad_handler_behavior(TriState::kEnabled);

    options = SelfProcessSnapshotAndGetCrashpadOptions();
    EXPECT_EQ(TriState::kEnabled, options.crashpad_handler_behavior);
    EXPECT_EQ(TriState::kUnset, options.system_crash_reporter_forwarding);
    EXPECT_EQ(TriState::kUnset, options.gather_indirectly_referenced_memory);
    EXPECT_EQ(0u, options.indirectly_referenced_memory_cap);
  }

  {
    ScopedUnsetCrashpadInfoOptions unset(crashpad_info);

    crashpad_info->set_system_crash_reporter_forwarding(TriState::kDisabled);

    options = SelfProcessSnapshotAndGetCrashpadOptions();
    EXPECT_EQ(TriState::kUnset, options.crashpad_handler_behavior);
    EXPECT_EQ(TriState::kDisabled, options.system_crash_reporter_forwarding);
    EXPECT_EQ(TriState::kUnset, options.gather_indirectly_referenced_memory);
    EXPECT_EQ(0u, options.indirectly_referenced_memory_cap);
  }

  {
    ScopedUnsetCrashpadInfoOptions unset(crashpad_info);

    crashpad_info->set_gather_indirectly_referenced_memory(TriState::kEnabled,
                                                           1234);

    options = SelfProcessSnapshotAndGetCrashpadOptions();
    EXPECT_EQ(TriState::kUnset, options.crashpad_handler_behavior);
    EXPECT_EQ(TriState::kUnset, options.system_crash_reporter_forwarding);
    EXPECT_EQ(TriState::kEnabled, options.gather_indirectly_referenced_memory);
    EXPECT_EQ(1234u, options.indirectly_referenced_memory_cap);
  }
}

#if defined(OS_POSIX)
using DlHandle = void*;
#elif defined(OS_WIN)
using DlHandle = HMODULE;
#endif  // OS_POSIX

class ScopedDlHandle {
 public:
  explicit ScopedDlHandle(DlHandle dl_handle)
      : dl_handle_(dl_handle) {
  }

  ~ScopedDlHandle() {
    if (dl_handle_) {
#if defined(OS_POSIX)
      if (dlclose(dl_handle_) != 0) {
        LOG(ERROR) << "dlclose: " << dlerror();
      }
#elif defined(OS_WIN)
      if (!FreeLibrary(dl_handle_))
        PLOG(ERROR) << "FreeLibrary";
#endif  // OS_POSIX
    }
  }

  bool valid() const { return dl_handle_ != nullptr; }

  template <typename T>
  T LookUpSymbol(const char* symbol_name) {
#if defined(OS_POSIX)
    return reinterpret_cast<T>(dlsym(dl_handle_, symbol_name));
#elif defined(OS_WIN)
    return reinterpret_cast<T>(GetProcAddress(dl_handle_, symbol_name));
#endif  // OS_POSIX
  }

 private:
  DlHandle dl_handle_;

  DISALLOW_COPY_AND_ASSIGN(ScopedDlHandle);
};

TEST(CrashpadInfoClientOptions, TwoModules) {
  // Open the module, which has its own CrashpadInfo structure.
#if defined(OS_MACOSX)
  const base::FilePath::StringType kDlExtension = FILE_PATH_LITERAL(".so");
#elif defined(OS_WIN)
  const base::FilePath::StringType kDlExtension = FILE_PATH_LITERAL(".dll");
#endif
  base::FilePath module_path = Paths::Executable().DirName().Append(
      FILE_PATH_LITERAL("crashpad_snapshot_test_module") + kDlExtension);
#if defined(OS_MACOSX)
  ScopedDlHandle dl_handle(
      dlopen(module_path.value().c_str(), RTLD_LAZY | RTLD_LOCAL));
  ASSERT_TRUE(dl_handle.valid()) << "dlopen " << module_path.value() << ": "
                                 << dlerror();
#elif defined(OS_WIN)
  ScopedDlHandle dl_handle(LoadLibrary(module_path.value().c_str()));
  ASSERT_TRUE(dl_handle.valid())
      << "LoadLibrary " << base::UTF16ToUTF8(module_path.value()) << ": "
      << ErrorMessage();
#else
#error Port.
#endif  // OS_MACOSX

  // Get the function pointer from the module. This wraps GetCrashpadInfo(), but
  // because it runs in the module, it returns the remote module’s CrashpadInfo
  // structure.
  CrashpadInfo* (*TestModule_GetCrashpadInfo)() =
      dl_handle.LookUpSymbol<CrashpadInfo* (*)()>("TestModule_GetCrashpadInfo");
  ASSERT_TRUE(TestModule_GetCrashpadInfo);

  auto options = SelfProcessSnapshotAndGetCrashpadOptions();

  // Make sure that the initial state has all values unset.
  EXPECT_EQ(TriState::kUnset, options.crashpad_handler_behavior);
  EXPECT_EQ(TriState::kUnset, options.system_crash_reporter_forwarding);
  EXPECT_EQ(TriState::kUnset, options.gather_indirectly_referenced_memory);

  // Get both CrashpadInfo structures.
  CrashpadInfo* local_crashpad_info = CrashpadInfo::GetCrashpadInfo();
  ASSERT_TRUE(local_crashpad_info);

  CrashpadInfo* remote_crashpad_info = TestModule_GetCrashpadInfo();
  ASSERT_TRUE(remote_crashpad_info);

  {
    ScopedUnsetCrashpadInfoOptions unset_local(local_crashpad_info);
    ScopedUnsetCrashpadInfoOptions unset_remote(remote_crashpad_info);

    // When only one module sets a value, it applies to the entire process.
    remote_crashpad_info->set_crashpad_handler_behavior(TriState::kEnabled);

    options = SelfProcessSnapshotAndGetCrashpadOptions();
    EXPECT_EQ(TriState::kEnabled, options.crashpad_handler_behavior);
    EXPECT_EQ(TriState::kUnset, options.system_crash_reporter_forwarding);
    EXPECT_EQ(TriState::kUnset, options.gather_indirectly_referenced_memory);

    // When more than one module sets a value, the first one in the module list
    // applies to the process. The local module should appear before the remote
    // module, because the local module loaded the remote module.
    local_crashpad_info->set_crashpad_handler_behavior(TriState::kDisabled);

    options = SelfProcessSnapshotAndGetCrashpadOptions();
    EXPECT_EQ(TriState::kDisabled, options.crashpad_handler_behavior);
    EXPECT_EQ(TriState::kUnset, options.system_crash_reporter_forwarding);
    EXPECT_EQ(TriState::kUnset, options.gather_indirectly_referenced_memory);
  }

  {
    ScopedUnsetCrashpadInfoOptions unset_local(local_crashpad_info);
    ScopedUnsetCrashpadInfoOptions unset_remote(remote_crashpad_info);

    // When only one module sets a value, it applies to the entire process.
    remote_crashpad_info->set_system_crash_reporter_forwarding(
        TriState::kDisabled);

    options = SelfProcessSnapshotAndGetCrashpadOptions();
    EXPECT_EQ(TriState::kUnset, options.crashpad_handler_behavior);
    EXPECT_EQ(TriState::kDisabled, options.system_crash_reporter_forwarding);
    EXPECT_EQ(TriState::kUnset, options.gather_indirectly_referenced_memory);

    // When more than one module sets a value, the first one in the module list
    // applies to the process. The local module should appear before the remote
    // module, because the local module loaded the remote module.
    local_crashpad_info->set_system_crash_reporter_forwarding(
        TriState::kEnabled);

    options = SelfProcessSnapshotAndGetCrashpadOptions();
    EXPECT_EQ(TriState::kUnset, options.crashpad_handler_behavior);
    EXPECT_EQ(TriState::kEnabled, options.system_crash_reporter_forwarding);
    EXPECT_EQ(TriState::kUnset, options.gather_indirectly_referenced_memory);
  }
}

}  // namespace
}  // namespace test
}  // namespace crashpad
