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

#include "util/linux/auxiliary_vector.h"

#include <linux/auxvec.h>
#include <unistd.h>

#include "base/macros.h"
#include "gtest/gtest.h"
#include "test/errors.h"
#include "test/multiprocess.h"
#include "util/linux/address_types.h"
#include "util/linux/memory_map.h"
#include "util/linux/process_memory.h"
#include "util/misc/from_pointer_cast.h"

#if defined(ARCH_CPU_ARMEL)
#include <sys/utsname.h>
#endif

extern "C" {
extern void _start();
}  // extern "C"

namespace crashpad {
namespace test {
namespace {

void TestAgainstCloneOrSelf(pid_t pid) {
#if defined(ARCH_CPU_64_BITS)
  constexpr bool am_64_bit = true;
#else
  constexpr bool am_64_bit = false;
#endif
  AuxiliaryVector aux;
  ASSERT_TRUE(aux.Initialize(pid, am_64_bit));

  MemoryMap mappings;
  ASSERT_TRUE(mappings.Initialize(pid));

  LinuxVMAddress phdrs;
  ASSERT_TRUE(aux.GetValue(AT_PHDR, &phdrs));
  EXPECT_TRUE(mappings.FindMapping(phdrs));

  int pagesize;
  ASSERT_TRUE(aux.GetValue(AT_PAGESZ, &pagesize));
  EXPECT_EQ(pagesize, getpagesize());

  LinuxVMAddress interp_base;
  ASSERT_TRUE(aux.GetValue(AT_BASE, &interp_base));
  EXPECT_TRUE(mappings.FindMapping(interp_base));

  LinuxVMAddress entry_addr;
  ASSERT_TRUE(aux.GetValue(AT_ENTRY, &entry_addr));
  EXPECT_EQ(entry_addr, FromPointerCast<LinuxVMAddress>(_start));

  uid_t uid;
  ASSERT_TRUE(aux.GetValue(AT_UID, &uid));
  EXPECT_EQ(uid, getuid());

  uid_t euid;
  ASSERT_TRUE(aux.GetValue(AT_EUID, &euid));
  EXPECT_EQ(euid, geteuid());

  gid_t gid;
  ASSERT_TRUE(aux.GetValue(AT_GID, &gid));
  EXPECT_EQ(gid, getgid());

  gid_t egid;
  ASSERT_TRUE(aux.GetValue(AT_EGID, &egid));
  EXPECT_EQ(egid, getegid());

  ProcessMemory memory;
  ASSERT_TRUE(memory.Initialize(pid));

  LinuxVMAddress platform_addr;
  ASSERT_TRUE(aux.GetValue(AT_PLATFORM, &platform_addr));
  std::string platform;
  ASSERT_TRUE(memory.ReadCStringSizeLimited(platform_addr, 10, &platform));
#if defined(ARCH_CPU_X86_64)
  EXPECT_STREQ(platform.c_str(), "x86_64");
#elif defined(ARCH_CPU_ARM64)
  EXPECT_STREQ(platform.c_str(), "aarch64");
#elif defined(ARCH_CPU_X86)
  EXPECT_STREQ(platform.c_str(), "i686");
#elif defined(ARCH_CPU_ARMEL)
  utsname sys_names;
  ASSERT_EQ(uname(&sys_names), 0);
  std::string machine_name(sys_names.machine);
  EXPECT_NE(machine_name.find(platform), std::string::npos);
#endif  // ARCH_CPU_X86_64

#if defined(AT_SYSINFO_EHDR)
  LinuxVMAddress vdso_addr;
  ASSERT_TRUE(aux.GetValue(AT_SYSINFO_EHDR, &vdso_addr));
  EXPECT_TRUE(mappings.FindMapping(vdso_addr));
#endif  // AT_SYSINFO_EHDR

#if defined(AT_EXECFN)
  LinuxVMAddress filename_addr;
  ASSERT_TRUE(aux.GetValue(AT_EXECFN, &filename_addr));
  std::string filename;
  EXPECT_TRUE(memory.ReadCStringSizeLimited(filename_addr, 4096, &filename));
  EXPECT_TRUE(filename.find("crashpad_util_test") != std::string::npos);
#endif  // AT_EXECFN

  int ignore;
  EXPECT_FALSE(aux.GetValue(AT_NULL, &ignore));

  char too_small;
  EXPECT_FALSE(aux.GetValue(AT_PAGESZ, &too_small));
}

TEST(AuxiliaryVector, ReadSelf) {
  TestAgainstCloneOrSelf(getpid());
}

class ReadChildTest : public Multiprocess {
 public:
  ReadChildTest() : Multiprocess() {}
  ~ReadChildTest() {}

 private:
  void MultiprocessParent() override { TestAgainstCloneOrSelf(ChildPID()); }

  void MultiprocessChild() override { CheckedReadFileAtEOF(ReadPipeHandle()); }

  DISALLOW_COPY_AND_ASSIGN(ReadChildTest);
};

TEST(AuxiliaryVector, ReadChild) {
  ReadChildTest test;
  test.Run();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
