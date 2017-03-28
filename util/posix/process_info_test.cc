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

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "gtest/gtest.h"
#include "test/errors.h"
#include "test/main_arguments.h"
#include "test/multiprocess.h"
#include "util/file/file_io.h"
#include "util/misc/implicit_cast.h"

namespace crashpad {
namespace test {
namespace {

void TestProcessSelfOrClone(const ProcessInfo& process_info) {
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

  bool is_64_bit;
  ASSERT_TRUE(process_info.Is64Bit(&is_64_bit));
#if defined(ARCH_CPU_64_BITS)
  EXPECT_TRUE(is_64_bit);
#else
  EXPECT_FALSE(is_64_bit);
#endif

  // Test StartTime(). This program must have started at some time in the past.
  timeval start_time;
  ASSERT_TRUE(process_info.StartTime(&start_time));
  EXPECT_FALSE(start_time.tv_sec == 0 && start_time.tv_usec == 0);
  time_t now;
  time(&now);
  EXPECT_LE(start_time.tv_sec, now);

  std::vector<std::string> argv;
  ASSERT_TRUE(process_info.Arguments(&argv));

  const std::vector<std::string>& expect_argv = GetMainArguments();

  // expect_argv always contains the initial view of the arguments at the time
  // the program was invoked. argv may contain this view, or it may contain the
  // current view of arguments after gtest argv processing. argv may be a subset
  // of expect_argv.
  //
  // gtest argv processing always leaves argv[0] intact, so this can be checked
  // directly.
  ASSERT_FALSE(expect_argv.empty());
  ASSERT_FALSE(argv.empty());
  EXPECT_EQ(expect_argv[0], argv[0]);

  EXPECT_LE(argv.size(), expect_argv.size());

  // Everything else in argv should have a match in expect_argv too, but things
  // may have moved around.
  for (size_t arg_index = 1; arg_index < argv.size(); ++arg_index) {
    const std::string& arg = argv[arg_index];
    SCOPED_TRACE(
        base::StringPrintf("arg_index %zu, arg %s", arg_index, arg.c_str()));
    EXPECT_NE(expect_argv.end(), std::find(argv.begin(), argv.end(), arg));
  }
}

void TestSelfProcess(const ProcessInfo& process_info) {
  EXPECT_EQ(getpid(), process_info.ProcessID());
  EXPECT_EQ(getppid(), process_info.ParentProcessID());

  TestProcessSelfOrClone(process_info);
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

class ProcessInfoForkedTest : public Multiprocess {
 public:
  ProcessInfoForkedTest() : Multiprocess() {}
  ~ProcessInfoForkedTest() {}

  // Multiprocess:
  void MultiprocessParent() override {
    const pid_t pid = ChildPID();

    ProcessInfo process_info;
    ASSERT_TRUE(process_info.Initialize(pid));

    EXPECT_EQ(pid, process_info.ProcessID());
    EXPECT_EQ(getpid(), process_info.ParentProcessID());

    TestProcessSelfOrClone(process_info);
  }

  void MultiprocessChild() override {
    // Hang around until the parent is done.
    CheckedReadFileAtEOF(ReadPipeHandle());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ProcessInfoForkedTest);
};

TEST(ProcessInfo, Forked) {
  ProcessInfoForkedTest test;
  test.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
