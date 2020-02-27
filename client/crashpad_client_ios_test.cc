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

#include "client/crashpad_client.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "gtest/gtest.h"
#include "test/scoped_temp_dir.h"
#include "test/test_paths.h"

namespace crashpad {
namespace test {
namespace {

TEST(CrashpadClient, DumpWithoutCrash) {
  // Start up crashpad.
  crashpad::CrashpadClient client;
  client.StartCrashpadInProcessHandler();
  client.DumpWithoutCrash(nullptr);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
