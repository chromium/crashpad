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

#include "util/linux/process_memory.h"

#include <limits.h>
#include <signal.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#include <memory>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "gtest/gtest.h"
#include "test/errors.h"
#include "test/multiprocess.h"
#include "util/posix/scoped_mmap.h"

namespace crashpad {
namespace test {
namespace {

class ScopedChildProcess {
 public:
  ScopedChildProcess() : pid_(-1) {}
  ~ScopedChildProcess() {
    if (pid_ > 0) {
      int kill_rv = kill(pid_, SIGKILL);
      if (kill_rv != 0) {
        EXPECT_EQ(0, kill_rv) << ErrnoMessage("kill");
        return;
      }
      int exit_status = 0;
      pid_t waitpid_rv = HANDLE_EINTR(waitpid(pid_, &exit_status, 0));
      if (pid_ != waitpid_rv) {
        EXPECT_EQ(pid_, waitpid_rv) << ErrnoMessage("waitpid");
        if (waitpid_rv < 0) {
          return;
        }
      }
      EXPECT_TRUE(WIFSIGNALED(exit_status));
      EXPECT_EQ(SIGKILL, WTERMSIG(exit_status));
    }
  }

  bool Initialize() {
    pid_ = fork();
    if (pid_ < 0) {
      PLOG(ERROR) << "fork";
      return false;
    }
    if (pid_ == 0) {
      raise(SIGSTOP);
      _exit(0);
    }
    return true;
  }

  pid_t pid() const { return pid_; }

 private:
  pid_t pid_;

  DISALLOW_COPY_AND_ASSIGN(ScopedChildProcess);
};

class TargetProcessTest : public Multiprocess {
 public:
  TargetProcessTest() : Multiprocess() {}
  ~TargetProcessTest() {}

  void RunAgainstTarget(pid_t pid) {
    DoTest(pid);
  }

  void RunAgainstForked() {
    Run();
  }

 private:
  void MultiprocessParent() override {
    DoTest(ChildPID());
  }

  void MultiprocessChild() override {
    CheckedReadFileAtEOF(ReadPipeHandle());
  }

  virtual void DoTest(pid_t pid) = 0;

  DISALLOW_COPY_AND_ASSIGN(TargetProcessTest);
};

class ReadTest : public TargetProcessTest {
 public:
  ReadTest() : TargetProcessTest() {
    region_.reset(new char[kSize]);
    if (!region_.get()) {
      return;
    }

    for (size_t index = 0; index < kSize; ++index) {
      region_[index] = index % 256;
    }
  }

  void DoTest(pid_t pid) override {
    ASSERT_TRUE(region_.get());
    LinuxVMAddress address = reinterpret_cast<LinuxVMAddress>(region_.get());

    ProcessMemory memory;
    ASSERT_TRUE(memory.Initialize(pid));

    std::unique_ptr<char[]> result(new char[kSize]);
    ASSERT_TRUE(result.get());

    // Ensure that the entire region can be read.
    ASSERT_TRUE(memory.Read(address, kSize, result.get()));
    EXPECT_EQ(0, memcmp(region_.get(), result.get(), kSize));

    // Ensure that a read of length 0 succeeds and doesn’t touch the result.
    memset(result.get(), '\0', kSize);
    ASSERT_TRUE(memory.Read(address, 0, result.get()));
    for (size_t i = 0; i < kSize; ++i) {
      EXPECT_EQ(0, result[i]);
    }

    // Ensure that a read starting at an unaligned address works.
    ASSERT_TRUE(memory.Read(address + 1, kSize - 1, result.get()));
    EXPECT_EQ(0, memcmp(region_.get() + 1, result.get(), kSize - 1));

    // Ensure that a read ending at an unaligned address works.
    ASSERT_TRUE(memory.Read(address, kSize - 1, result.get()));
    EXPECT_EQ(0, memcmp(region_.get(), result.get(), kSize - 1));

    // Ensure that a read starting and ending at unaligned addresses works.
    ASSERT_TRUE(memory.Read(address + 1, kSize - 2, result.get()));
    EXPECT_EQ(0, memcmp(region_.get() + 1, result.get(), kSize - 2));

    // Ensure that a read of exactly one page works.
    ASSERT_TRUE(memory.Read(address + page_size, page_size, result.get()));
    EXPECT_EQ(0, memcmp(region_.get() + page_size, result.get(), page_size));

    // Ensure that reading exactly a single byte works.
    result[1] = 'J';
    ASSERT_TRUE(memory.Read(address + 2, 1, result.get()));
    EXPECT_EQ(region_[2], result[0]);
    EXPECT_EQ('J', result[1]);
  }

 private:
  std::unique_ptr<char[]> region_;
  static const size_t page_size = 4096;
  static const size_t kSize = 4 * page_size;
};

TEST(ProcessMemory, ReadSelfTest) {
  ReadTest test;
  test.RunAgainstTarget(getpid());
}

TEST(ProcessMemory, ReadForkedTest) {
  ReadTest test;
  test.RunAgainstForked();
}

void TestRead(bool do_fork) {
  const size_t page_size = 4096;
  const size_t kSize = 4 * page_size;
  std::unique_ptr<char[]> region(new char[kSize]);
  ASSERT_TRUE(region.get());
  LinuxVMAddress address = reinterpret_cast<LinuxVMAddress>(region.get());

  for (size_t index = 0; index < kSize; ++index) {
    region[index] = index % 256;
  }

  ScopedChildProcess child;
  pid_t pid;
  if (do_fork) {
    ASSERT_TRUE(child.Initialize());
    pid = child.pid();
  } else {
    pid = getpid();
  }

  ProcessMemory memory;
  ASSERT_TRUE(memory.Initialize(pid));

  std::unique_ptr<char[]> result(new char[kSize]);
  ASSERT_TRUE(result.get());

  // Ensure that the entire region can be read.
  ASSERT_TRUE(memory.Read(address, kSize, result.get()));
  EXPECT_EQ(0, memcmp(region.get(), result.get(), kSize));

  // Ensure that a read of length 0 succeeds and doesn’t touch the result.
  memset(result.get(), '\0', kSize);
  ASSERT_TRUE(memory.Read(address, 0, result.get()));
  for (size_t i = 0; i < kSize; ++i) {
    EXPECT_EQ(0, result[i]);
  }

  // Ensure that a read starting at an unaligned address works.
  ASSERT_TRUE(memory.Read(address + 1, kSize - 1, result.get()));
  EXPECT_EQ(0, memcmp(region.get() + 1, result.get(), kSize - 1));

  // Ensure that a read ending at an unaligned address works.
  ASSERT_TRUE(memory.Read(address, kSize - 1, result.get()));
  EXPECT_EQ(0, memcmp(region.get(), result.get(), kSize - 1));

  // Ensure that a read starting and ending at unaligned addresses works.
  ASSERT_TRUE(memory.Read(address + 1, kSize - 2, result.get()));
  EXPECT_EQ(0, memcmp(region.get() + 1, result.get(), kSize - 2));

  // Ensure that a read of exactly one page works.
  ASSERT_TRUE(memory.Read(address + page_size, page_size, result.get()));
  EXPECT_EQ(0, memcmp(region.get() + page_size, result.get(), page_size));

  // Ensure that reading exactly a single byte works.
  result[1] = 'J';
  ASSERT_TRUE(memory.Read(address + 2, 1, result.get()));
  EXPECT_EQ(region[2], result[0]);
  EXPECT_EQ('J', result[1]);
}

TEST(ProcessMemory, ReadSelf) {
  TestRead(/* do_fork= */ false);
}

TEST(ProcessMemory, ReadForked) {
  TestRead(/* do_fork= */ true);
}

bool ReadCString(const ProcessMemory& memory,
                 const char* pointer,
                 std::string* result) {
  return memory.ReadCString(reinterpret_cast<LinuxVMAddress>(pointer), result);
}

void TestReadCString(bool do_fork) {
  const char kConstCharEmpty[] = "";
  const char kConstCharShort[] = "A short const char[]";
  static const char kStaticConstCharEmpty[] = "";
  static const char kStaticConstCharShort[] = "A short static const char[]";
  std::string string_long;
  const size_t kStringLongSize = 4 * 4096;
  for (size_t index = 0; index < kStringLongSize; ++index) {
    string_long.push_back((index % 255) + 1);
  }
  EXPECT_EQ(kStringLongSize, string_long.size());

  ScopedChildProcess child;
  pid_t pid;
  if (do_fork) {
    ASSERT_TRUE(child.Initialize());
    pid = child.pid();
  } else {
    pid = getpid();
  }

  ProcessMemory memory;
  ASSERT_TRUE(memory.Initialize(pid));

  std::string result;

  ASSERT_TRUE(ReadCString(memory, kConstCharEmpty, &result));
  EXPECT_EQ(kConstCharEmpty, result);

  ASSERT_TRUE(ReadCString(memory, kConstCharShort, &result));
  EXPECT_EQ(kConstCharShort, result);

  ASSERT_TRUE(ReadCString(memory, kStaticConstCharEmpty, &result));
  EXPECT_EQ(kStaticConstCharEmpty, result);

  ASSERT_TRUE(ReadCString(memory, kStaticConstCharShort, &result));
  EXPECT_EQ(kStaticConstCharShort, result);

  ASSERT_TRUE(ReadCString(memory, string_long.c_str(), &result));
  EXPECT_EQ(string_long, result);
}

TEST(ProcessMemory, ReadCStringSelf) {
  TestReadCString(/* do_fork= */ false);
}

TEST(ProcessMemory, ReadCStringForked) {
  TestReadCString(/* do_fork= */ true);
}

bool ReadCStringSizeLimited(const ProcessMemory& memory,
                            const char* pointer,
                            size_t size,
                            std::string* result) {
  return memory.ReadCStringSizeLimited(
      reinterpret_cast<LinuxVMAddress>(pointer), size, result);
}

void TestReadCStringSizeLimited(bool do_fork) {
  const char kConstCharEmpty[] = "";
  const char kConstCharShort[] = "A short const char[]";
  static const char kStaticConstCharEmpty[] = "";
  static const char kStaticConstCharShort[] = "A short static const char[]";
  std::string string_long;
  const size_t kStringLongSize = 4 * 4096;
  for (size_t index = 0; index < kStringLongSize; ++index) {
    string_long.push_back((index % 255) + 1);
  }
  EXPECT_EQ(kStringLongSize, string_long.size());

  ScopedChildProcess child;
  pid_t pid;
  if (do_fork) {
    ASSERT_TRUE(child.Initialize());
    pid = child.pid();
  } else {
    pid = getpid();
  }

  ProcessMemory memory;
  ASSERT_TRUE(memory.Initialize(pid));

  std::string result;

  ASSERT_TRUE(ReadCStringSizeLimited(
      memory, kConstCharEmpty, arraysize(kConstCharEmpty), &result));
  EXPECT_EQ(kConstCharEmpty, result);

  ASSERT_TRUE(ReadCStringSizeLimited(
      memory, kConstCharShort, arraysize(kConstCharShort), &result));
  EXPECT_EQ(kConstCharShort, result);
  EXPECT_FALSE(ReadCStringSizeLimited(
      memory, kConstCharShort, arraysize(kConstCharShort) - 1, &result));

  ASSERT_TRUE(ReadCStringSizeLimited(memory,
                                     kStaticConstCharEmpty,
                                     arraysize(kStaticConstCharEmpty),
                                     &result));
  EXPECT_EQ(kStaticConstCharEmpty, result);

  ASSERT_TRUE(ReadCStringSizeLimited(memory,
                                     kStaticConstCharShort,
                                     arraysize(kStaticConstCharShort),
                                     &result));
  EXPECT_EQ(kStaticConstCharShort, result);
  EXPECT_FALSE(ReadCStringSizeLimited(memory,
                                      kStaticConstCharShort,
                                      arraysize(kStaticConstCharShort) - 1,
                                      &result));

  ASSERT_TRUE(ReadCStringSizeLimited(
      memory, string_long.c_str(), string_long.size() + 1, &result));
  EXPECT_EQ(string_long, result);
  EXPECT_FALSE(ReadCStringSizeLimited(
      memory, string_long.c_str(), string_long.size(), &result));
}

TEST(ProcessMemory, ReadCStringSizeLimitedSelf) {
  TestReadCStringSizeLimited(/* do_fork= */ false);
}

TEST(ProcessMemory, ReadCStringSizeLimitedForked) {
  TestReadCStringSizeLimited(/* do_fork= */ true);
}

void TestReadUnmapped(bool do_fork) {
  int page_size = getpagesize();
  ASSERT_GT(page_size, 0);

  const size_t kSize = 2 * page_size;
  void* region = mmap(nullptr,
                      kSize,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS,
                      -1,
                      0);
  ASSERT_NE(MAP_FAILED, region) << ErrnoMessage("mmap");
  ScopedMmap pages;
  pages.ResetAddrLen(region, kSize);

  for (size_t index = 0; index < kSize; ++index) {
    char* region_c = static_cast<char*>(region);
    region_c[index] = index % 256;
  }

  std::unique_ptr<char[]> result(new char[kSize]);
  ASSERT_TRUE(result.get());

  LinuxVMAddress page_addr1 = reinterpret_cast<LinuxVMAddress>(region);
  LinuxVMAddress page_addr2 = page_addr1 + page_size;
  ASSERT_EQ(0, munmap(reinterpret_cast<void*>(page_addr2), page_size))
      << ErrnoMessage("munmap");

  ScopedChildProcess child;
  pid_t pid;
  if (do_fork) {
    ASSERT_TRUE(child.Initialize());
    pid = child.pid();
  } else {
    pid = getpid();
  }

  ProcessMemory memory;
  ASSERT_TRUE(memory.Initialize(pid));

  EXPECT_FALSE(memory.Read(page_addr1, kSize, result.get()));
  EXPECT_TRUE(memory.Read(page_addr1, page_size, result.get()));
}

TEST(ProcessMemory, ReadUnmappedSelf) {
  TestReadUnmapped(/* do_fork= */ false);
}

TEST(ProcessMemory, ReadUnmappedForked) {
  TestReadUnmapped(/* do_fork= */ true);
}

void TestReadCStringUnmapped(bool do_fork, bool limit_size) {
  int page_size = getpagesize();
  ASSERT_GT(page_size, 0);

  const size_t kSize = 2 * page_size;
  void* region = mmap(nullptr,
                      kSize,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS,
                      -1,
                      0);
  ASSERT_NE(MAP_FAILED, region) << ErrnoMessage("mmap");
  ScopedMmap pages;
  pages.ResetAddrLen(region, kSize);


  char* region_c = static_cast<char*>(region);
  for (size_t index = 0; index < kSize; ++index) {
    region_c[index] = 1 + index % 255;
  }
  const size_t length = 10;
  // A string at the start of the mapped region
  char* string1 = region_c;
  string1[length] = '\0';
  // A string near the end of the mapped region
  char* string2 = region_c + page_size - 20;
  string2[length] = '\0';
  // A string that crosses from the mapped into the unmapped region
  char* string3 = region_c + page_size - 9;
  string3[length] = '\0';
  // A string entirely in the unmapped region
  char* string4 = region_c + page_size + 10;
  string4[length] = '\0';

  std::string result;
  result.reserve(length + 1);

  LinuxVMAddress page_addr1 = reinterpret_cast<LinuxVMAddress>(region);
  LinuxVMAddress page_addr2 = page_addr1 + page_size;
  ASSERT_EQ(0, munmap(reinterpret_cast<void*>(page_addr2), page_size))
      << ErrnoMessage("munmap");

  ScopedChildProcess child;
  pid_t pid;
  if (do_fork) {
    ASSERT_TRUE(child.Initialize());
    pid = child.pid();
  } else {
    pid = getpid();
  }

  ProcessMemory memory;
  ASSERT_TRUE(memory.Initialize(pid));

  if (limit_size) {
    ASSERT_TRUE(ReadCStringSizeLimited(memory, string1, length + 1, &result));
    EXPECT_EQ(string1, result);
    ASSERT_TRUE(ReadCStringSizeLimited(memory, string2, length + 1, &result));
    EXPECT_EQ(string2, result);
    EXPECT_FALSE(ReadCStringSizeLimited(memory, string3, length + 1, &result));
    EXPECT_FALSE(ReadCStringSizeLimited(memory, string4, length + 1, &result));
  } else {
    ASSERT_TRUE(ReadCString(memory, string1, &result));
    EXPECT_EQ(string1, result);
    ASSERT_TRUE(ReadCString(memory, string2, &result));
    EXPECT_EQ(string2, result);
    EXPECT_FALSE(ReadCString(memory, string3, &result));
    EXPECT_FALSE(ReadCString(memory, string4, &result));
  }
}

TEST(ProcessMemory, ReadCStringUnmappedSelf) {
  TestReadCStringUnmapped(/* do_fork= */ false, /* limit_size= */ false);
}

TEST(ProcessMemory, ReadCStringUnmappedForked) {
  TestReadCStringUnmapped(/* do_fork= */ true, /* limit_size= */ false);
}

TEST(ProcessMemory, ReadCStringSizeLimitedUnmappedSelf) {
  TestReadCStringUnmapped(/* do_fork= */ false, /* limit_size= */ true);
}

TEST(ProcessMemory, ReadCStringSizeLimitedUnmappedForked) {
  TestReadCStringUnmapped(/* do_fork= */ true, /* limit_size= */ true);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
