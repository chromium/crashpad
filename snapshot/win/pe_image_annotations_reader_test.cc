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

#include "snapshot/win/pe_image_annotations_reader.h"

#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "client/crashpad_info.h"
#include "client/simple_string_dictionary.h"
#include "gtest/gtest.h"
#include "snapshot/win/pe_image_reader.h"
#include "snapshot/win/process_reader_win.h"
#include "test/paths.h"
#include "test/win/child_launcher.h"
#include "util/file/file_io.h"
#include "util/win/process_info.h"

namespace crashpad {
namespace test {
namespace {

enum TestType {
  // Don't crash, just test the CrashpadInfo interface.
  kDontCrash = 0,

  // The child process should crash by __debugbreak().
  kCrashDebugBreak,
};

void TestAnnotationsOnCrash(TestType type,
                            const base::string16& directory_modification) {
  // Spawn a child process, passing it the pipe name to connect to.
  base::FilePath test_executable = Paths::Executable();
  std::wstring child_test_executable =
      test_executable.DirName()
          .Append(directory_modification)
          .Append(test_executable.BaseName().RemoveFinalExtension().value() +
                  L"_simple_annotations.exe")
          .value();
  ChildLauncher child(child_test_executable, L"");
  child.Start();

  // Wait for the child process to indicate that it's done setting up its
  // annotations via the CrashpadInfo interface.
  char c;
  CheckedReadFile(child.stdout_read_handle(), &c, sizeof(c));

  ProcessReaderWin process_reader;
  ASSERT_TRUE(process_reader.Initialize(child.process_handle(),
                                        ProcessSuspensionState::kRunning));

  // Verify the "simple map" annotations set via the CrashpadInfo interface.
  const std::vector<ProcessInfo::Module>& modules = process_reader.Modules();
  std::map<std::string, std::string> all_annotations_simple_map;
  for (const ProcessInfo::Module& module : modules) {
    PEImageReader pe_image_reader;
    pe_image_reader.Initialize(&process_reader,
                               module.dll_base,
                               module.size,
                               base::UTF16ToUTF8(module.name));
    PEImageAnnotationsReader module_annotations_reader(
        &process_reader, &pe_image_reader, module.name);
    std::map<std::string, std::string> module_annotations_simple_map =
        module_annotations_reader.SimpleMap();
    all_annotations_simple_map.insert(module_annotations_simple_map.begin(),
                                      module_annotations_simple_map.end());
  }

  EXPECT_GE(all_annotations_simple_map.size(), 5u);
  EXPECT_EQ("crash", all_annotations_simple_map["#TEST# pad"]);
  EXPECT_EQ("value", all_annotations_simple_map["#TEST# key"]);
  EXPECT_EQ("y", all_annotations_simple_map["#TEST# x"]);
  EXPECT_EQ("shorter", all_annotations_simple_map["#TEST# longer"]);
  EXPECT_EQ("", all_annotations_simple_map["#TEST# empty_value"]);

  // Tell the child process to continue.
  DWORD expected_exit_code;
  switch (type) {
    case kDontCrash:
      c = ' ';
      expected_exit_code = 0;
      break;
    case kCrashDebugBreak:
      c = 'd';
      expected_exit_code = STATUS_BREAKPOINT;
      break;
    default:
      FAIL();
  }
  CheckedWriteFile(child.stdin_write_handle(), &c, sizeof(c));

  EXPECT_EQ(expected_exit_code, child.WaitForExit());
}

TEST(PEImageAnnotationsReader, DontCrash) {
  TestAnnotationsOnCrash(kDontCrash, FILE_PATH_LITERAL("."));
}

TEST(PEImageAnnotationsReader, CrashDebugBreak) {
  TestAnnotationsOnCrash(kCrashDebugBreak, FILE_PATH_LITERAL("."));
}

#if defined(ARCH_CPU_64_BITS)
TEST(PEImageAnnotationsReader, DontCrashWOW64) {
#ifndef NDEBUG
  TestAnnotationsOnCrash(kDontCrash, FILE_PATH_LITERAL("..\\..\\out\\Debug"));
#else
  TestAnnotationsOnCrash(kDontCrash, FILE_PATH_LITERAL("..\\..\\out\\Release"));
#endif
}

TEST(PEImageAnnotationsReader, CrashDebugBreakWOW64) {
#ifndef NDEBUG
  TestAnnotationsOnCrash(kCrashDebugBreak,
                         FILE_PATH_LITERAL("..\\..\\out\\Debug"));
#else
  TestAnnotationsOnCrash(kCrashDebugBreak,
                         FILE_PATH_LITERAL("..\\..\\out\\Release"));
#endif
}
#endif  // ARCH_CPU_64_BITS

}  // namespace
}  // namespace test
}  // namespace crashpad
