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

#include "snapshot/win/process_snapshot_win.h"

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "snapshot/win/module_snapshot_win.h"
#include "snapshot/win/pe_image_reader.h"
#include "snapshot/win/process_reader_win.h"
#include "test/paths.h"
#include "test/win/child_launcher.h"
#include "util/file/file_io.h"
#include "util/win/scoped_handle.h"
#include "util/win/scoped_process_suspend.h"

namespace crashpad {
namespace test {
namespace {

void TestImageReaderChild(const base::string16& directory_modification) {
  UUID done_uuid(UUID::InitializeWithNewTag{});
  ScopedKernelHANDLE done(
      CreateEvent(nullptr, true, false, done_uuid.ToString16().c_str()));
  ASSERT_TRUE(done.get());

  base::FilePath test_executable = Paths::Executable();
  std::wstring child_test_executable =
      test_executable.DirName()
          .Append(directory_modification)
          .Append(test_executable.BaseName().RemoveFinalExtension().value() +
                  L"_image_reader.exe")
          .value();
  ChildLauncher child(child_test_executable, done_uuid.ToString16());
  child.Start();

  char c;
  ASSERT_TRUE(LoggingReadFile(child.stdout_read_handle(), &c, sizeof(c)));
  ASSERT_EQ(' ', c);

  ScopedProcessSuspend suspend(child.process_handle());

  ProcessSnapshotWin process_snapshot;
  ASSERT_TRUE(process_snapshot.Initialize(
      child.process_handle(), ProcessSuspensionState::kSuspended, 0));

  ASSERT_GE(process_snapshot.Modules().size(), 2u);

  UUID uuid;
  DWORD age;
  std::string pdbname;
  const std::string suffix(".pdb");

  // Check the main .exe to see that we can retrieve its sections.
  auto module = reinterpret_cast<const internal::ModuleSnapshotWin*>(
      process_snapshot.Modules()[0]);
  ASSERT_TRUE(module->pe_image_reader().DebugDirectoryInformation(
      &uuid, &age, &pdbname));
  EXPECT_NE(std::string::npos,
            pdbname.find("crashpad_snapshot_test_image_reader"));
  EXPECT_EQ(
      0,
      pdbname.compare(pdbname.size() - suffix.size(), suffix.size(), suffix));

  // Check the dll it loads too.
  module = reinterpret_cast<const internal::ModuleSnapshotWin*>(
      process_snapshot.Modules().back());
  ASSERT_TRUE(module->pe_image_reader().DebugDirectoryInformation(
      &uuid, &age, &pdbname));
  EXPECT_NE(std::string::npos,
            pdbname.find("crashpad_snapshot_test_image_reader_module"));
  EXPECT_EQ(
      0,
      pdbname.compare(pdbname.size() - suffix.size(), suffix.size(), suffix));

  // Tell the child it can terminate.
  SetEvent(done.get());
}

TEST(ProcessSnapshotTest, CrashpadInfoChild) {
  TestImageReaderChild(FILE_PATH_LITERAL("."));
}

#if defined(ARCH_CPU_64_BITS)
TEST(ProcessSnapshotTest, CrashpadInfoChildWOW64) {
#ifndef NDEBUG
  TestImageReaderChild(FILE_PATH_LITERAL("..\\..\\out\\Debug"));
#else
  TestImageReaderChild(FILE_PATH_LITERAL("..\\..\\out\\Release"));
#endif
}
#endif

}  // namespace
}  // namespace test
}  // namespace crashpad
