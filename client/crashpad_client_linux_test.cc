// Copyright 2018 The Crashpad Authors. All rights reserved.
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

#include "client/crashpad_client.h"

#include <stdlib.h>

#include "base/logging.h"
#include "base/macros.h"
#include "client/crash_report_database.h"
#include "gtest/gtest.h"
#include "test/multiprocess_exec.h"
#include "test/scoped_temp_dir.h"
#include "test/test_paths.h"
#include "util/file/file_io.h"
#include "util/misc/address_types.h"

namespace crashpad {
namespace test {
namespace {

CRASHPAD_CHILD_TEST_MAIN(StartHandlerAtCrashChild) {
  FileHandle in = StdioFileHandle(StdioStream::kStandardInput);

  VMSize temp_dir_length;
  CheckedReadFileExactly(in, &temp_dir_length, sizeof(temp_dir_length));

  std::string temp_dir(temp_dir_length, '\0');
  CheckedReadFileExactly(in, &temp_dir[0], temp_dir_length);

  base::FilePath handler_path = TestPaths::Executable().DirName().Append(
      FILE_PATH_LITERAL("crashpad_handler"));

  crashpad::CrashpadClient client;
  if (!client.StartHandlerAtCrash(handler_path,
                             base::FilePath(temp_dir),
                             base::FilePath(),
                             "",
                             std::map<std::string, std::string>(),
                             std::vector<std::string>())) {
    return EXIT_FAILURE;
  }

  *(reinterpret_cast<volatile int*>(0)) = 42;

  return EXIT_SUCCESS;
}

class StartHandlerAtCrashTest : public MultiprocessExec {
 public:
  StartHandlerAtCrashTest() : MultiprocessExec() {
    SetChildTestMainFunction("StartHandlerAtCrashChild");
  }

 private:
  void MultiprocessParent() override {
    ScopedTempDir temp_dir;
    auto database = CrashReportDatabase::BuildDatabase(temp_dir.path(), /* may_create= */ true);

    std::vector<CrashReportDatabase::Report> reports;
    ASSERT_EQ(database->GetPendingReports(&reports), CrashReportDatabase::kNoError);
    EXPECT_EQ(reports.size(), 0u);

    ASSERT_EQ(database->GetCompletedReports(&reports), CrashReportDatabase::kNoError);
    EXPECT_EQ(reports.size(), 0u);

    VMSize temp_dir_length = temp_dir.path().value().size();
    ASSERT_TRUE(LoggingWriteFile(WritePipeHandle(), &temp_dir_length, sizeof(temp_dir_length)));

    ASSERT_TRUE(LoggingWriteFile(WritePipeHandle(), temp_dir.path().value().data(), temp_dir_length));

    // TODO timed wait?
    CheckedReadFileAtEOF(ReadPipeHandle());

    ASSERT_EQ(database->GetPendingReports(&reports), CrashReportDatabase::kNoError);
    EXPECT_EQ(reports.size(), 0u);

    ASSERT_EQ(database->GetCompletedReports(&reports), CrashReportDatabase::kNoError);
    EXPECT_EQ(reports.size(), 1u);
  }

  DISALLOW_COPY_AND_ASSIGN(StartHandlerAtCrashTest);
};

TEST(CrashpadClient, StartHandlerAtCrash) {
  StartHandlerAtCrashTest test;
  test.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
