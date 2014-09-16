// Copyright 2014 The Crashpad Authors. All rights reserved.
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

#include "util/mac/service_management.h"

#import <Foundation/Foundation.h>
#include <launch.h>
#include <time.h>

#include <string>
#include <vector>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/rand_util.h"
#include "gtest/gtest.h"
#include "util/mac/mac_util.h"
#include "util/posix/process_util.h"
#include "util/stdlib/objc.h"

namespace {

using namespace crashpad;

// Ensures that the process with the specified PID is running, identifying it by
// requiring that its argv[argc - 1] compare equal to last_arg.
void ExpectProcessIsRunning(pid_t pid, std::string& last_arg) {
  // The process may not have called exec yet, so loop with a small delay while
  // looking for the cookie.
  int outer_tries = 10;
  std::vector<std::string> job_argv;
  while (outer_tries--) {
    // If the process is in the middle of calling exec, ProcessArgumentsForPID
    // may fail. Loop with a small retry delay while waiting for the expected
    // successful call.
    int inner_tries = 10;
    bool success;
    do {
      success = ProcessArgumentsForPID(pid, &job_argv);
      if (success) {
        break;
      }
      if (inner_tries > 0) {
        timespec sleep_time;
        sleep_time.tv_sec = 0;
        sleep_time.tv_nsec = 1E5;  // 100 microseconds
        nanosleep(&sleep_time, NULL);
      }
    } while (inner_tries--);
    ASSERT_TRUE(success);

    ASSERT_TRUE(ProcessArgumentsForPID(pid, &job_argv));
    ASSERT_FALSE(job_argv.empty());
    if (job_argv.back() == last_arg) {
      break;
    }

    if (outer_tries > 0) {
      timespec sleep_time;
      sleep_time.tv_sec = 0;
      sleep_time.tv_nsec = 1E6;  // 1 millisecond
      nanosleep(&sleep_time, NULL);
    }
  }

  ASSERT_FALSE(job_argv.empty());
  EXPECT_EQ(last_arg, job_argv.back());
}

// Ensures that the process with the specified PID is not running. Because the
// PID may be reused for another process, a process is only treated as running
// if its argv[argc - 1] compares equal to last_arg.
void ExpectProcessIsNotRunning(pid_t pid, std::string& last_arg) {
  // The process may not have exited yet, so loop with a small delay while
  // checking that it has exited.
  int tries = 10;
  std::vector<std::string> job_argv;
  while (tries--) {
    if (!ProcessArgumentsForPID(pid, &job_argv)) {
      // The PID was not found.
      return;
    }

    // The PID was found. It may have been recycled for another process. Make
    // sure that the cookie isn’t found.
    ASSERT_FALSE(job_argv.empty());
    if (job_argv.back() != last_arg) {
      break;
    }

    if (tries > 0) {
      timespec sleep_time;
      sleep_time.tv_sec = 0;
      sleep_time.tv_nsec = 1E6;  // 1 millisecond
      nanosleep(&sleep_time, NULL);
    }
  }

  ASSERT_FALSE(job_argv.empty());
  EXPECT_NE(last_arg, job_argv.back());
}

TEST(ServiceManagement, SubmitRemoveJob) {
  @autoreleasepool {
    std::string cookie;
    for (int index = 0; index < 16; ++index) {
      cookie.append(1, base::RandInt('A', 'Z'));
    }

    std::string shell_script =
        base::StringPrintf("sleep 10; echo %s", cookie.c_str());
    NSString* shell_script_ns = base::SysUTF8ToNSString(shell_script);

    const char kJobLabel[] = "com.googlecode.crashpad.test.service_management";
    NSDictionary* job_dictionary_ns = @{
      @LAUNCH_JOBKEY_LABEL : @"com.googlecode.crashpad.test.service_management",
      @LAUNCH_JOBKEY_RUNATLOAD : @YES,
      @LAUNCH_JOBKEY_PROGRAMARGUMENTS :
          @[ @"/bin/sh", @"-c", shell_script_ns, ],
    };
    CFDictionaryRef job_dictionary_cf =
        base::mac::NSToCFCast(job_dictionary_ns);

    // The job may be left over from a failed previous run.
    if (ServiceManagementIsJobLoaded(kJobLabel)) {
      EXPECT_TRUE(ServiceManagementRemoveJob(kJobLabel, true));
    }

    EXPECT_FALSE(ServiceManagementIsJobLoaded(kJobLabel));
    ASSERT_FALSE(ServiceManagementIsJobRunning(kJobLabel));

    // Submit the job.
    ASSERT_TRUE(ServiceManagementSubmitJob(job_dictionary_cf));
    EXPECT_TRUE(ServiceManagementIsJobLoaded(kJobLabel));

    // launchd started the job because RunAtLoad is true.
    pid_t job_pid = ServiceManagementIsJobRunning(kJobLabel);
    ASSERT_GT(job_pid, 0);

    ExpectProcessIsRunning(job_pid, shell_script);

    // Remove the job.
    ASSERT_TRUE(ServiceManagementRemoveJob(kJobLabel, true));
    EXPECT_FALSE(ServiceManagementIsJobLoaded(kJobLabel));
    EXPECT_EQ(0, ServiceManagementIsJobRunning(kJobLabel));

    // Now that the job is unloaded, a subsequent attempt to unload it should be
    // an error. However, ServiceManagementRemoveJob does not properly report
    // this error case on Mac OS X 10.10.
    if (MacOSXMinorVersion() >= 10) {
      // If this check starts failing because radar 18268941 is fixed, remove
      // the OS version check here and revise the interface documentation for
      // ServiceManagementRemoveJob().
      EXPECT_TRUE(ServiceManagementRemoveJob(kJobLabel, false));
    } else {
      EXPECT_FALSE(ServiceManagementRemoveJob(kJobLabel, false));
    }

    ExpectProcessIsNotRunning(job_pid, shell_script);
  }
}

}  // namespace
