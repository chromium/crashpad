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

#include "gtest/gtest.h"
#include "test/gtest_disabled.h"
#include "test/main_arguments.h"

#if defined(CRASHPAD_IN_CHROMIUM)
#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#endif  // CRASHPAD_IN_CHROMIUM

int main(int argc, char* argv[]) {
  crashpad::test::InitializeMainArguments(argc, argv);
  testing::InitGoogleTest(&argc, argv);
  testing::AddGlobalTestEnvironment(
      crashpad::test::DisabledTestGtestEnvironment::Get());

#if defined(CRASHPAD_IN_CHROMIUM)

  // This supports --test-launcher-summary-output, which writes a JSON file
  // containing test details needed by Swarming.
  base::TestSuite test_suite(argc, argv);
  return base::LaunchUnitTests(
      argc,
      argv,
      base::Bind(&base::TestSuite::Run, base::Unretained(&test_suite)));

#else  // CRASHPAD_IN_CHROMIUM

  return RUN_ALL_TESTS();

#endif  // CRASHPAD_IN_CHROMIUM
}
