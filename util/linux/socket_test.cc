// Copyright 2019 The Crashpad Authors. All rights reserved.
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

#include "util/linux/socket.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "gtest/gtest.h"
#include "test/multiprocess_exec.h"
#include "third_party/lss/lss.h"
#include "util/linux/socket.h"
#include "util/synchronization/semaphore.h"
#include "util/thread/thread.h"

namespace crashpad {
namespace test {
namespace {

TEST(Socket, Self) {
  ADD_FAILURE();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
