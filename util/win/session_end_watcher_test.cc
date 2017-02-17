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

#include "util/win/session_end_watcher.h"

#include "gtest/gtest.h"
#include "test/errors.h"

namespace crashpad {
namespace test {
namespace {

class SessionEndWatcherTest final : public SessionEndWatcher {
 public:
  SessionEndWatcherTest()
      : SessionEndWatcher(),
        started_(CreateEvent(nullptr, false, false, nullptr)),
        stopped_(CreateEvent(nullptr, false, false, nullptr)),
        called_(false) {
    EXPECT_TRUE(started_.get()) << ErrorMessage("CreateEvent");
    EXPECT_TRUE(stopped_.get()) << ErrorMessage("CreateEvent");
  }

  ~SessionEndWatcherTest() override {}

  void Run(DWORD message) {
    ASSERT_EQ(WAIT_OBJECT_0, WaitForSingleObject(started_.get(), INFINITE))
        << ErrorMessage("WaitForSingleObject");

    HWND window = GetWindow();
    ASSERT_TRUE(window);
    EXPECT_TRUE(PostMessage(window, message, 0, 0));

    ASSERT_EQ(WAIT_OBJECT_0, WaitForSingleObject(stopped_.get(), INFINITE))
        << ErrorMessage("WaitForSingleObject");

    EXPECT_TRUE(called_);
  }

 protected:
  // SessionEndWatcher:
  void SessionEndWatcherEvent(Event event) override {
    switch (event) {
      case Event::kStartedWatching:
        EXPECT_TRUE(SetEvent(started_.get())) << ErrorMessage("SetEvent");
        break;

      case Event::kSessionEnding:
        called_ = true;
        break;

      case Event::kStoppedWatching:
        EXPECT_TRUE(SetEvent(stopped_.get())) << ErrorMessage("SetEvent");
        break;
    }
  }

 private:
  ScopedKernelHANDLE started_;
  ScopedKernelHANDLE stopped_;
  bool called_;
};

TEST(SessionEndWatcher, Close) {
  SessionEndWatcherTest test;
  test.Run(WM_CLOSE);
}

TEST(SessionEndWatcher, EndSession) {
  SessionEndWatcherTest test;
  test.Run(WM_ENDSESSION);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
