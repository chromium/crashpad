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

#include "snapshot/mac/crashpad_info_client_options.h"

#include <dlfcn.h>

#include "base/files/file_path.h"
#include "client/crashpad_info.h"
#include "gtest/gtest.h"
#include "snapshot/mac/process_snapshot_mac.h"
#include "util/test/paths.h"

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
  }

 private:
  CrashpadInfo* crashpad_info_;

  DISALLOW_COPY_AND_ASSIGN(ScopedUnsetCrashpadInfoOptions);
};

TEST(CrashpadInfoClientOptions, OneModule) {
  // Make sure that the initial state has all values unset.
  ProcessSnapshotMac process_snapshot;
  ASSERT_TRUE(process_snapshot.Initialize(mach_task_self()));

  CrashpadInfoClientOptions options;
  process_snapshot.GetCrashpadOptions(&options);

  EXPECT_EQ(TriState::kUnset, options.crashpad_handler_behavior);
  EXPECT_EQ(TriState::kUnset, options.system_crash_reporter_forwarding);

  CrashpadInfo* crashpad_info = CrashpadInfo::GetCrashpadInfo();
  ASSERT_TRUE(crashpad_info);

  {
    ScopedUnsetCrashpadInfoOptions unset(crashpad_info);

    crashpad_info->set_crashpad_handler_behavior(TriState::kEnabled);

    process_snapshot.GetCrashpadOptions(&options);
    EXPECT_EQ(TriState::kEnabled, options.crashpad_handler_behavior);
    EXPECT_EQ(TriState::kUnset, options.system_crash_reporter_forwarding);
  }

  {
    ScopedUnsetCrashpadInfoOptions unset(crashpad_info);

    crashpad_info->set_system_crash_reporter_forwarding(TriState::kDisabled);

    process_snapshot.GetCrashpadOptions(&options);
    EXPECT_EQ(TriState::kUnset, options.crashpad_handler_behavior);
    EXPECT_EQ(TriState::kDisabled, options.system_crash_reporter_forwarding);
  }
}

class ScopedDlHandle {
 public:
  explicit ScopedDlHandle(void* dl_handle)
      : dl_handle_(dl_handle) {
  }

  ~ScopedDlHandle() {
    if (dl_handle_) {
      if (dlclose(dl_handle_) != 0) {
        LOG(ERROR) << "dlclose: " << dlerror();
      }
    }
  }

  bool valid() const { return dl_handle_ != nullptr; }

  template <typename T>
  T LookUpSymbol(const char* symbol_name) {
    return reinterpret_cast<T>(dlsym(dl_handle_, symbol_name));
  }

 private:
  void* dl_handle_;

  DISALLOW_COPY_AND_ASSIGN(ScopedDlHandle);
};

TEST(CrashpadInfoClientOptions, TwoModules) {
  // Open the module, which has its own CrashpadInfo structure.
  base::FilePath module_path =
      Paths::Executable().DirName().Append("crashpad_snapshot_test_module.so");
  ScopedDlHandle dl_handle(
      dlopen(module_path.value().c_str(), RTLD_LAZY | RTLD_LOCAL));
  ASSERT_TRUE(dl_handle.valid()) << "dlopen " << module_path.value() << ": "
                                 << dlerror();

  // Get the function pointer from the module. This wraps GetCrashpadInfo(), but
  // because it runs in the module, it returns the remote module’s CrashpadInfo
  // structure.
  CrashpadInfo* (*TestModule_GetCrashpadInfo)() =
      dl_handle.LookUpSymbol<CrashpadInfo* (*)()>("TestModule_GetCrashpadInfo");
  ASSERT_TRUE(TestModule_GetCrashpadInfo);

  // Make sure that the initial state has all values unset.
  ProcessSnapshotMac process_snapshot;
  ASSERT_TRUE(process_snapshot.Initialize(mach_task_self()));

  CrashpadInfoClientOptions options;
  process_snapshot.GetCrashpadOptions(&options);

  EXPECT_EQ(TriState::kUnset, options.crashpad_handler_behavior);
  EXPECT_EQ(TriState::kUnset, options.system_crash_reporter_forwarding);

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

    process_snapshot.GetCrashpadOptions(&options);
    EXPECT_EQ(TriState::kEnabled, options.crashpad_handler_behavior);
    EXPECT_EQ(TriState::kUnset, options.system_crash_reporter_forwarding);

    // When more than one module sets a value, the first one in the module list
    // applies to the process. The local module should appear before the remote
    // module, because the local module loaded the remote module.
    local_crashpad_info->set_crashpad_handler_behavior(TriState::kDisabled);

    process_snapshot.GetCrashpadOptions(&options);
    EXPECT_EQ(TriState::kDisabled, options.crashpad_handler_behavior);
    EXPECT_EQ(TriState::kUnset, options.system_crash_reporter_forwarding);
  }

  {
    ScopedUnsetCrashpadInfoOptions unset_local(local_crashpad_info);
    ScopedUnsetCrashpadInfoOptions unset_remote(remote_crashpad_info);

    // When only one module sets a value, it applies to the entire process.
    remote_crashpad_info->set_system_crash_reporter_forwarding(
        TriState::kDisabled);

    process_snapshot.GetCrashpadOptions(&options);
    EXPECT_EQ(TriState::kUnset, options.crashpad_handler_behavior);
    EXPECT_EQ(TriState::kDisabled, options.system_crash_reporter_forwarding);

    // When more than one module sets a value, the first one in the module list
    // applies to the process. The local module should appear before the remote
    // module, because the local module loaded the remote module.
    local_crashpad_info->set_system_crash_reporter_forwarding(
        TriState::kEnabled);

    process_snapshot.GetCrashpadOptions(&options);
    EXPECT_EQ(TriState::kUnset, options.crashpad_handler_behavior);
    EXPECT_EQ(TriState::kEnabled, options.system_crash_reporter_forwarding);
  }
}

}  // namespace
}  // namespace test
}  // namespace crashpad
