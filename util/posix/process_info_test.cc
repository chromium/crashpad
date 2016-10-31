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

#include "util/posix/process_info.h"

#include <time.h>
#include <stdio.h>
#include <unistd.h>

#include <set>
#include <string>
#include <vector>

#include "base/files/scoped_file.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "test/errors.h"
#include "util/misc/implicit_cast.h"

#if defined(OS_MACOSX)
#include <crt_externs.h>
#endif

namespace crashpad {
namespace test {
namespace {

void TestSelfProcess(const ProcessInfo& process_info) {
  EXPECT_EQ(getpid(), process_info.ProcessID());
  EXPECT_EQ(getppid(), process_info.ParentProcessID());

  // There’s no system call to obtain the saved set-user ID or saved set-group
  // ID in an easy way. Normally, they are the same as the effective user ID and
  // effective group ID, so just check against those.
  EXPECT_EQ(getuid(), process_info.RealUserID());
  const uid_t euid = geteuid();
  EXPECT_EQ(euid, process_info.EffectiveUserID());
  EXPECT_EQ(euid, process_info.SavedUserID());
  const gid_t gid = getgid();
  EXPECT_EQ(gid, process_info.RealGroupID());
  const gid_t egid = getegid();
  EXPECT_EQ(egid, process_info.EffectiveGroupID());
  EXPECT_EQ(egid, process_info.SavedGroupID());

  // Test SupplementaryGroups().
  int group_count = getgroups(0, nullptr);
  ASSERT_GE(group_count, 0) << ErrnoMessage("getgroups");

  std::vector<gid_t> group_vector(group_count);
  if (group_count > 0) {
    group_count = getgroups(group_vector.size(), &group_vector[0]);
    ASSERT_GE(group_count, 0) << ErrnoMessage("getgroups");
    ASSERT_EQ(group_vector.size(), implicit_cast<size_t>(group_count));
  }

  std::set<gid_t> group_set(group_vector.begin(), group_vector.end());
  EXPECT_EQ(group_set, process_info.SupplementaryGroups());

  // Test AllGroups(), which is SupplementaryGroups() plus the real, effective,
  // and saved set-group IDs. The effective and saved set-group IDs are expected
  // to be identical (see above).
  group_set.insert(gid);
  group_set.insert(egid);

  EXPECT_EQ(group_set, process_info.AllGroups());

  // The test executable isn’t expected to change privileges.
  EXPECT_FALSE(process_info.DidChangePrivileges());

#if defined(ARCH_CPU_64_BITS)
  EXPECT_TRUE(process_info.Is64Bit());
#else
  EXPECT_FALSE(process_info.Is64Bit());
#endif

  // Test StartTime(). This program must have started at some time in the past.
  timeval start_time;
  process_info.StartTime(&start_time);
  time_t now;
  time(&now);
  EXPECT_LE(start_time.tv_sec, now);

  std::vector<std::string> argv;
  ASSERT_TRUE(process_info.Arguments(&argv));

  // gtest argv processing scrambles argv, but it leaves argc and argv[0]
  // intact, so test those.

#if defined(OS_MACOSX)
  int expect_argc = *_NSGetArgc();
  char** expect_argv = *_NSGetArgv();
#elif defined(OS_LINUX) || defined(OS_ANDROID)
  std::vector<std::string> expect_arg_vector;
  {
    base::ScopedFILE cmdline(fopen("/proc/self/cmdline", "r"));
    ASSERT_NE(nullptr, cmdline.get()) << ErrnoMessage("fopen");

    int expect_arg_char;
    std::string expect_arg_string;
    while ((expect_arg_char = fgetc(cmdline.get())) != EOF) {
      if (expect_arg_char != '\0') {
        expect_arg_string.append(1, expect_arg_char);
      } else {
        expect_arg_vector.push_back(expect_arg_string);
        expect_arg_string.clear();
      }
    }
    ASSERT_EQ(0, ferror(cmdline.get())) << ErrnoMessage("fgetc");
    ASSERT_TRUE(expect_arg_string.empty());
  }

  std::vector<const char*> expect_argv_storage;
  for (const std::string& expect_arg_string : expect_arg_vector) {
    expect_argv_storage.push_back(expect_arg_string.c_str());
  }

  int expect_argc = expect_argv_storage.size();
  const char* const* expect_argv =
      !expect_argv_storage.empty() ? &expect_argv_storage[0] : nullptr;
#else
#error Obtain expect_argc and expect_argv correctly on your system.
#endif

  int argc = implicit_cast<int>(argv.size());
  EXPECT_EQ(expect_argc, argc);

  ASSERT_GE(expect_argc, 1);
  ASSERT_GE(argc, 1);

  EXPECT_EQ(std::string(expect_argv[0]), argv[0]);
}


TEST(ProcessInfo, Self) {
  ProcessInfo process_info;
  ASSERT_TRUE(process_info.Initialize(getpid()));
  TestSelfProcess(process_info);
}

#if defined(OS_MACOSX)
TEST(ProcessInfo, SelfTask) {
  ProcessInfo process_info;
  ASSERT_TRUE(process_info.InitializeFromTask(mach_task_self()));
  TestSelfProcess(process_info);
}
#endif

TEST(ProcessInfo, Pid1) {
  // PID 1 is expected to be init or the system’s equivalent. This tests reading
  // information about another process.
  ProcessInfo process_info;
  ASSERT_TRUE(process_info.Initialize(1));

  EXPECT_EQ(implicit_cast<pid_t>(1), process_info.ProcessID());
  EXPECT_EQ(implicit_cast<pid_t>(0), process_info.ParentProcessID());
  EXPECT_EQ(implicit_cast<uid_t>(0), process_info.RealUserID());
  EXPECT_EQ(implicit_cast<uid_t>(0), process_info.EffectiveUserID());
  EXPECT_EQ(implicit_cast<uid_t>(0), process_info.SavedUserID());
  EXPECT_EQ(implicit_cast<gid_t>(0), process_info.RealGroupID());
  EXPECT_EQ(implicit_cast<gid_t>(0), process_info.EffectiveGroupID());
  EXPECT_EQ(implicit_cast<gid_t>(0), process_info.SavedGroupID());
  EXPECT_FALSE(process_info.AllGroups().empty());
}

}  // namespace
}  // namespace test
}  // namespace crashpad
