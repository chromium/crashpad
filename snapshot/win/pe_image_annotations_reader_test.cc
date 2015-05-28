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

#include "base/basictypes.h"
#include "base/strings/utf_string_conversions.h"
#include "client/crashpad_info.h"
#include "client/simple_string_dictionary.h"
#include "gtest/gtest.h"
#include "snapshot/win/pe_image_reader.h"
#include "snapshot/win/process_reader_win.h"
#include "test/win/win_multiprocess.h"
#include "util/file/file_io.h"
#include "util/win/process_info.h"

namespace crashpad {
namespace test {
namespace {

class TestPEImageAnnotationsReader final : public WinMultiprocess {
 public:
  enum TestType {
    // Don't crash, just test the CrashpadInfo interface.
    kDontCrash = 0,

    // The child process should crash by __debugbreak().
    kCrashDebugBreak,
  };

  explicit TestPEImageAnnotationsReader(TestType test_type)
      : WinMultiprocess(), test_type_(test_type) {}

  ~TestPEImageAnnotationsReader() {}

 private:
  // WinMultiprocess:

  void WinMultiprocessParent() override {
    ProcessReaderWin process_reader;
    ASSERT_TRUE(process_reader.Initialize(ChildProcess()));

    // Wait for the child process to indicate that it's done setting up its
    // annotations via the CrashpadInfo interface.
    char c;
    CheckedReadFile(ReadPipeHandle(), &c, sizeof(c));

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

    if (test_type_ == kCrashDebugBreak)
      SetExpectedChildExitCode(STATUS_BREAKPOINT);

    // Tell the child process to continue.
    CheckedWriteFile(WritePipeHandle(), &c, sizeof(c));
  }

  void WinMultiprocessChild() override {
    CrashpadInfo* crashpad_info = CrashpadInfo::GetCrashpadInfo();

    // This is "leaked" to crashpad_info.
    SimpleStringDictionary* simple_annotations = new SimpleStringDictionary();
    simple_annotations->SetKeyValue("#TEST# pad", "break");
    simple_annotations->SetKeyValue("#TEST# key", "value");
    simple_annotations->SetKeyValue("#TEST# pad", "crash");
    simple_annotations->SetKeyValue("#TEST# x", "y");
    simple_annotations->SetKeyValue("#TEST# longer", "shorter");
    simple_annotations->SetKeyValue("#TEST# empty_value", "");

    crashpad_info->set_simple_annotations(simple_annotations);

    // Tell the parent that the environment has been set up.
    char c = '\0';
    CheckedWriteFile(WritePipeHandle(), &c, sizeof(c));

    // Wait for the parent to indicate that it's safe to continue/crash.
    CheckedReadFile(ReadPipeHandle(), &c, sizeof(c));

    switch (test_type_) {
      case kDontCrash:
        break;

      case kCrashDebugBreak:
        __debugbreak();
        break;
    }
  }

  TestType test_type_;

  DISALLOW_COPY_AND_ASSIGN(TestPEImageAnnotationsReader);
};

TEST(PEImageAnnotationsReader, DontCrash) {
  TestPEImageAnnotationsReader test_pe_image_annotations_reader(
      TestPEImageAnnotationsReader::kDontCrash);
  test_pe_image_annotations_reader.Run();
}

TEST(PEImageAnnotationsReader, CrashDebugBreak) {
  TestPEImageAnnotationsReader test_pe_image_annotations_reader(
      TestPEImageAnnotationsReader::kCrashDebugBreak);
  test_pe_image_annotations_reader.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
