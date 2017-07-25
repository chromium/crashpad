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

#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <memory>

#include "gtest/gtest.h"
#include "test/errors.h"
#include "test/multiprocess.h"
#include "util/file/file_io.h"
#include "util/misc/from_pointer_cast.h"
#include "util/posix/scoped_mmap.h"

namespace crashpad {
namespace test {
namespace {

class TargetProcessTest : public Multiprocess {
 public:
  TargetProcessTest() : Multiprocess() {}
  ~TargetProcessTest() {}

  void RunAgainstSelf() { DoTest(getpid()); }

  void RunAgainstForked() { Run(); }

 private:
  void MultiprocessParent() override { DoTest(ChildPID()); }

  void MultiprocessChild() override { CheckedReadFileAtEOF(ReadPipeHandle()); }

  virtual void DoTest(pid_t pid) = 0;

  DISALLOW_COPY_AND_ASSIGN(TargetProcessTest);
};

class ReadTest : public TargetProcessTest {
 public:
  ReadTest()
      : TargetProcessTest(),
        page_size_(getpagesize()),
        region_size_(4 * page_size_),
        region_(new char[region_size_]) {
    for (size_t index = 0; index < region_size_; ++index) {
      region_[index] = index % 256;
    }
  }

 private:
  void DoTest(pid_t pid) override {
    ProcessMemory memory;
    ASSERT_TRUE(memory.Initialize(pid));

    LinuxVMAddress address = FromPointerCast<LinuxVMAddress>(region_.get());
    std::unique_ptr<char[]> result(new char[region_size_]);

    // Ensure that the entire region can be read.
    ASSERT_TRUE(memory.Read(address, region_size_, result.get()));
    EXPECT_EQ(memcmp(region_.get(), result.get(), region_size_), 0);

    // Ensure that a read of length 0 succeeds and doesn’t touch the result.
    memset(result.get(), '\0', region_size_);
    ASSERT_TRUE(memory.Read(address, 0, result.get()));
    for (size_t i = 0; i < region_size_; ++i) {
      EXPECT_EQ(result[i], 0);
    }

    // Ensure that a read starting at an unaligned address works.
    ASSERT_TRUE(memory.Read(address + 1, region_size_ - 1, result.get()));
    EXPECT_EQ(memcmp(region_.get() + 1, result.get(), region_size_ - 1), 0);

    // Ensure that a read ending at an unaligned address works.
    ASSERT_TRUE(memory.Read(address, region_size_ - 1, result.get()));
    EXPECT_EQ(memcmp(region_.get(), result.get(), region_size_ - 1), 0);

    // Ensure that a read starting and ending at unaligned addresses works.
    ASSERT_TRUE(memory.Read(address + 1, region_size_ - 2, result.get()));
    EXPECT_EQ(memcmp(region_.get() + 1, result.get(), region_size_ - 2), 0);

    // Ensure that a read of exactly one page works.
    ASSERT_TRUE(memory.Read(address + page_size_, page_size_, result.get()));
    EXPECT_EQ(memcmp(region_.get() + page_size_, result.get(), page_size_), 0);

    // Ensure that reading exactly a single byte works.
    result[1] = 'J';
    ASSERT_TRUE(memory.Read(address + 2, 1, result.get()));
    EXPECT_EQ(result[0], region_[2]);
    EXPECT_EQ(result[1], 'J');
  }

  const size_t page_size_;
  const size_t region_size_;
  std::unique_ptr<char[]> region_;

  DISALLOW_COPY_AND_ASSIGN(ReadTest);
};

TEST(ProcessMemory, ReadSelf) {
  ReadTest test;
  test.RunAgainstSelf();
}

TEST(ProcessMemory, ReadForked) {
  ReadTest test;
  test.RunAgainstForked();
}

bool ReadCString(const ProcessMemory& memory,
                 const char* pointer,
                 std::string* result) {
  return memory.ReadCString(FromPointerCast<LinuxVMAddress>(pointer), result);
}

bool ReadCStringSizeLimited(const ProcessMemory& memory,
                            const char* pointer,
                            size_t size,
                            std::string* result) {
  return memory.ReadCStringSizeLimited(
      FromPointerCast<LinuxVMAddress>(pointer), size, result);
}

constexpr char kConstCharEmpty[] = "";
constexpr char kConstCharShort[] = "A short const char[]";

class ReadCStringTest : public TargetProcessTest {
 public:
  ReadCStringTest(bool limit_size)
      : TargetProcessTest(),
        member_char_empty_(""),
        member_char_short_("A short member char[]"),
        limit_size_(limit_size) {
    const size_t kStringLongSize = 4 * getpagesize();
    for (size_t index = 0; index < kStringLongSize; ++index) {
      string_long_.push_back((index % 255) + 1);
    }
    EXPECT_EQ(string_long_.size(), kStringLongSize);
  }

 private:
  void DoTest(pid_t pid) override {
    ProcessMemory memory;
    ASSERT_TRUE(memory.Initialize(pid));

    std::string result;

    if (limit_size_) {
      ASSERT_TRUE(ReadCStringSizeLimited(
          memory, kConstCharEmpty, arraysize(kConstCharEmpty), &result));
      EXPECT_EQ(result, kConstCharEmpty);

      ASSERT_TRUE(ReadCStringSizeLimited(
          memory, kConstCharShort, arraysize(kConstCharShort), &result));
      EXPECT_EQ(result, kConstCharShort);
      EXPECT_FALSE(ReadCStringSizeLimited(
          memory, kConstCharShort, arraysize(kConstCharShort) - 1, &result));

      ASSERT_TRUE(ReadCStringSizeLimited(
          memory, member_char_empty_, strlen(member_char_empty_) + 1, &result));
      EXPECT_EQ(result, member_char_empty_);

      ASSERT_TRUE(ReadCStringSizeLimited(
          memory, member_char_short_, strlen(member_char_short_) + 1, &result));
      EXPECT_EQ(result, member_char_short_);
      EXPECT_FALSE(ReadCStringSizeLimited(
          memory, member_char_short_, strlen(member_char_short_), &result));

      ASSERT_TRUE(ReadCStringSizeLimited(
          memory, string_long_.c_str(), string_long_.size() + 1, &result));
      EXPECT_EQ(result, string_long_);
      EXPECT_FALSE(ReadCStringSizeLimited(
          memory, string_long_.c_str(), string_long_.size(), &result));
    } else {
      ASSERT_TRUE(ReadCString(memory, kConstCharEmpty, &result));
      EXPECT_EQ(result, kConstCharEmpty);

      ASSERT_TRUE(ReadCString(memory, kConstCharShort, &result));
      EXPECT_EQ(result, kConstCharShort);

      ASSERT_TRUE(ReadCString(memory, member_char_empty_, &result));
      EXPECT_EQ(result, member_char_empty_);

      ASSERT_TRUE(ReadCString(memory, member_char_short_, &result));
      EXPECT_EQ(result, member_char_short_);

      ASSERT_TRUE(ReadCString(memory, string_long_.c_str(), &result));
      EXPECT_EQ(result, string_long_);
    }
  }

  std::string string_long_;
  const char* member_char_empty_;
  const char* member_char_short_;
  const bool limit_size_;

  DISALLOW_COPY_AND_ASSIGN(ReadCStringTest);
};

TEST(ProcessMemory, ReadCStringSelf) {
  ReadCStringTest test(/* limit_size= */ false);
  test.RunAgainstSelf();
}

TEST(ProcessMemory, ReadCStringForked) {
  ReadCStringTest test(/* limit_size= */ false);
  test.RunAgainstForked();
}

TEST(ProcessMemory, ReadCStringSizeLimitedSelf) {
  ReadCStringTest test(/* limit_size= */ true);
  test.RunAgainstSelf();
}

TEST(ProcessMemory, ReadCStringSizeLimitedForked) {
  ReadCStringTest test(/* limit_size= */ true);
  test.RunAgainstForked();
}

class ReadUnmappedTest : public TargetProcessTest {
 public:
  ReadUnmappedTest()
      : TargetProcessTest(),
        page_size_(getpagesize()),
        region_size_(2 * page_size_),
        result_(new char[region_size_]) {
    if (!pages_.ResetMmap(nullptr,
                          region_size_,
                          PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS,
                          -1,
                          0)) {
      ADD_FAILURE();
      return;
    }

    char* region = pages_.addr_as<char*>();
    for (size_t index = 0; index < region_size_; ++index) {
      region[index] = index % 256;
    }

    EXPECT_TRUE(pages_.ResetAddrLen(region, page_size_));
  }

 private:
  void DoTest(pid_t pid) override {
    ProcessMemory memory;
    ASSERT_TRUE(memory.Initialize(pid));

    LinuxVMAddress page_addr1 = pages_.addr_as<LinuxVMAddress>();
    LinuxVMAddress page_addr2 = page_addr1 + page_size_;

    EXPECT_TRUE(memory.Read(page_addr1, page_size_, result_.get()));
    EXPECT_TRUE(memory.Read(page_addr2 - 1, 1, result_.get()));

    EXPECT_FALSE(memory.Read(page_addr1, region_size_, result_.get()));
    EXPECT_FALSE(memory.Read(page_addr2, page_size_, result_.get()));
    EXPECT_FALSE(memory.Read(page_addr2 - 1, 2, result_.get()));
  }

  ScopedMmap pages_;
  const size_t page_size_;
  const size_t region_size_;
  std::unique_ptr<char[]> result_;

  DISALLOW_COPY_AND_ASSIGN(ReadUnmappedTest);
};

TEST(ProcessMemory, ReadUnmappedSelf) {
  ReadUnmappedTest test;
  ASSERT_FALSE(testing::Test::HasFailure());
  test.RunAgainstSelf();
}

TEST(ProcessMemory, ReadUnmappedForked) {
  ReadUnmappedTest test;
  ASSERT_FALSE(testing::Test::HasFailure());
  test.RunAgainstForked();
}

class ReadCStringUnmappedTest : public TargetProcessTest {
 public:
  ReadCStringUnmappedTest(bool limit_size)
      : TargetProcessTest(),
        page_size_(getpagesize()),
        region_size_(2 * page_size_),
        limit_size_(limit_size) {
    if (!pages_.ResetMmap(nullptr,
                          region_size_,
                          PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS,
                          -1,
                          0)) {
      ADD_FAILURE();
      return;
    }

    char* region = pages_.addr_as<char*>();
    for (size_t index = 0; index < region_size_; ++index) {
      region[index] = 1 + index % 255;
    }

    // A string at the start of the mapped region
    string1_ = region;
    string1_[expected_length_] = '\0';

    // A string near the end of the mapped region
    string2_ = region + page_size_ - expected_length_ * 2;
    string2_[expected_length_] = '\0';

    // A string that crosses from the mapped into the unmapped region
    string3_ = region + page_size_ - expected_length_ + 1;
    string3_[expected_length_] = '\0';

    // A string entirely in the unmapped region
    string4_ = region + page_size_ + 10;
    string4_[expected_length_] = '\0';

    result_.reserve(expected_length_ + 1);

    EXPECT_TRUE(pages_.ResetAddrLen(region, page_size_));
  }

 private:
  void DoTest(pid_t pid) {
    ProcessMemory memory;
    ASSERT_TRUE(memory.Initialize(pid));

    if (limit_size_) {
      ASSERT_TRUE(ReadCStringSizeLimited(
          memory, string1_, expected_length_ + 1, &result_));
      EXPECT_EQ(result_, string1_);
      ASSERT_TRUE(ReadCStringSizeLimited(
          memory, string2_, expected_length_ + 1, &result_));
      EXPECT_EQ(result_, string2_);
      EXPECT_FALSE(ReadCStringSizeLimited(
          memory, string3_, expected_length_ + 1, &result_));
      EXPECT_FALSE(ReadCStringSizeLimited(
          memory, string4_, expected_length_ + 1, &result_));
    } else {
      ASSERT_TRUE(ReadCString(memory, string1_, &result_));
      EXPECT_EQ(result_, string1_);
      ASSERT_TRUE(ReadCString(memory, string2_, &result_));
      EXPECT_EQ(result_, string2_);
      EXPECT_FALSE(ReadCString(memory, string3_, &result_));
      EXPECT_FALSE(ReadCString(memory, string4_, &result_));
    }
  }

  std::string result_;
  ScopedMmap pages_;
  const size_t page_size_;
  const size_t region_size_;
  static const size_t expected_length_ = 10;
  char* string1_;
  char* string2_;
  char* string3_;
  char* string4_;
  const bool limit_size_;

  DISALLOW_COPY_AND_ASSIGN(ReadCStringUnmappedTest);
};

TEST(ProcessMemory, ReadCStringUnmappedSelf) {
  ReadCStringUnmappedTest test(/* limit_size= */ false);
  ASSERT_FALSE(testing::Test::HasFailure());
  test.RunAgainstSelf();
}

TEST(ProcessMemory, ReadCStringUnmappedForked) {
  ReadCStringUnmappedTest test(/* limit_size= */ false);
  ASSERT_FALSE(testing::Test::HasFailure());
  test.RunAgainstForked();
}

TEST(ProcessMemory, ReadCStringSizeLimitedUnmappedSelf) {
  ReadCStringUnmappedTest test(/* limit_size= */ true);
  ASSERT_FALSE(testing::Test::HasFailure());
  test.RunAgainstSelf();
}

TEST(ProcessMemory, ReadCStringSizeLimitedUnmappedForked) {
  ReadCStringUnmappedTest test(/* limit_size= */ true);
  ASSERT_FALSE(testing::Test::HasFailure());
  test.RunAgainstForked();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
