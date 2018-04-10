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

#include "base/logging.h"
#include "client/crash_report_database.h"
#include "client/simulate_crash.h"
#include "gtest/gtest.h"
#include "test/scoped_temp_dir.h"
#include "test/test_paths.h"

namespace crashpad {
namespace test {
namespace {

TEST(CrashpadClient, SimulateCrash) {
  ScopedTempDir temp_dir;

  base::FilePath handler_path = TestPaths::Executable().DirName().Append(
      FILE_PATH_LITERAL("crashpad_handler"));

  crashpad::CrashpadClient client;
  ASSERT_TRUE(client.StartHandlerAtCrash(handler_path,
                                         base::FilePath(temp_dir.path()),
                                         base::FilePath(),
                                         "",
                                         std::map<std::string, std::string>(),
                                         std::vector<std::string>()));

  auto database =
      CrashReportDatabase::InitializeWithoutCreating(temp_dir.path());
  ASSERT_TRUE(database);

  {
    CRASHPAD_SIMULATE_CRASH();

    std::vector<CrashReportDatabase::Report> reports;
    ASSERT_EQ(database->GetPendingReports(&reports),
              CrashReportDatabase::kNoError);
    EXPECT_EQ(reports.size(), 0u);

    reports.clear();
    ASSERT_EQ(database->GetCompletedReports(&reports),
              CrashReportDatabase::kNoError);
    EXPECT_EQ(reports.size(), 1u);
  }
}

}  // namespace
}  // namespace test
}  // namespace crashpad
