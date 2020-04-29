// Copyright 2020 The Crashpad Authors. All rights reserved.
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

#import <Foundation/Foundation.h>
#include <errno.h>
#include <spawn.h>
#include <stdio.h>
#include <unistd.h>

#include <vector>

#include "base/logging.h"
#include "gtest/gtest.h"
#include "test/test_paths.h"
#include "testing/platform_test.h"

extern "C" char** environ;

namespace crashpad {
namespace test {
namespace {

using CrashpadIOSClient = PlatformTest;

TEST_F(CrashpadIOSClient, DumpWithoutCrash) {
#if 0
  fprintf(stderr, "initial %d\n", getpid());
  pid_t pid = fork();
  if (pid == 0) {
    fprintf(stderr, "child %d\n", getpid());
  } else if (pid > 0) {
    fprintf(stderr, "parent, child %d\n", pid);
    _exit(0);
  } else {
    PLOG(ERROR) << "fork";
  }
  fflush(stderr);
#else
  base::FilePath executable = TestPaths::Executable();
  LOG(INFO) << "executable " << executable.value();
  base::FilePath auxiliary = executable.DirName().Append("auxiliary");
  LOG(INFO) << "auxiliary " << auxiliary.value();
  pid_t pid;
  std::string argv0 = auxiliary.BaseName().value();
  const char* const argv[] = {argv0.c_str(), nullptr};
  errno = posix_spawn(&pid,
                      auxiliary.value().c_str(),
                      nullptr,
                      nullptr,
                      const_cast<char**>(argv),
                      environ);
  if (errno != 0) {
    PLOG(ERROR) << "posix_spawn";
  } else {
    LOG(INFO) << "posix_spawn: pid=" << pid;
  }
#endif

  CrashpadClient client;
  client.StartCrashpadInProcessHandler();

  NativeCPUContext context;
#if defined(ARCH_CPU_X86_64)
  CaptureContext(&context);
#elif defined(ARCH_CPU_ARM64)
  // TODO(justincohen): Implement CaptureContext for ARM64.
  mach_msg_type_number_t thread_state_count = MACHINE_THREAD_STATE_COUNT;
  kern_return_t kr =
      thread_get_state(mach_thread_self(),
                       MACHINE_THREAD_STATE,
                       reinterpret_cast<thread_state_t>(&context),
                       &thread_state_count);
  ASSERT_EQ(kr, KERN_SUCCESS);
#endif
  client.DumpWithoutCrash(&context);
}

// This test is covered by a similar XCUITest, but for development purposes
// it's sometimes easier and faster to run as a gtest.  However, there's no
// way to correctly run this as a gtest. Leave the test here, disabled, for use
// during development only.
TEST_F(CrashpadIOSClient, DISABLED_ThrowNSException) {
  CrashpadClient client;
  client.StartCrashpadInProcessHandler();
  [NSException raise:@"GtestNSException" format:@"ThrowException"];
}

// This test is covered by a similar XCUITest, but for development purposes
// it's sometimes easier and faster to run as a gtest.  However, there's no
// way to correctly run this as a gtest. Leave the test here, disabled, for use
// during development only.
TEST_F(CrashpadIOSClient, DISABLED_ThrowException) {
  CrashpadClient client;
  client.StartCrashpadInProcessHandler();
  std::vector<int> empty_vector;
  empty_vector.at(42);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
