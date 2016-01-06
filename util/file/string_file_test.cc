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

#include "util/file/string_file.h"

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <limits>

#include "gtest/gtest.h"
#include "util/misc/implicit_cast.h"

namespace crashpad {
namespace test {
namespace {

TEST(StringFile, EmptyFile) {
  StringFile string_file;
  EXPECT_TRUE(string_file.string().empty());
  EXPECT_EQ(0, string_file.Seek(0, SEEK_CUR));
  EXPECT_TRUE(string_file.Write("", 0));
  EXPECT_TRUE(string_file.string().empty());
  EXPECT_EQ(0, string_file.Seek(0, SEEK_CUR));

  char c = '6';
  EXPECT_EQ(0, string_file.Read(&c, 1));
  EXPECT_EQ('6', c);
  EXPECT_EQ(0, string_file.Seek(0, SEEK_CUR));

  EXPECT_TRUE(string_file.string().empty());
}

TEST(StringFile, OneByteFile) {
  StringFile string_file;

  EXPECT_TRUE(string_file.Write("a", 1));
  EXPECT_EQ(1u, string_file.string().size());
  EXPECT_EQ("a", string_file.string());
  EXPECT_EQ(1, string_file.Seek(0, SEEK_CUR));
  EXPECT_EQ(0, string_file.Seek(0, SEEK_SET));
  char c = '6';
  EXPECT_EQ(1, string_file.Read(&c, 1));
  EXPECT_EQ('a', c);
  EXPECT_EQ(1, string_file.Seek(0, SEEK_CUR));
  EXPECT_EQ(0, string_file.Read(&c, 1));
  EXPECT_EQ('a', c);
  EXPECT_EQ(1, string_file.Seek(0, SEEK_CUR));
  EXPECT_EQ("a", string_file.string());

  EXPECT_EQ(0, string_file.Seek(0, SEEK_SET));
  EXPECT_TRUE(string_file.Write("b", 1));
  EXPECT_EQ(1u, string_file.string().size());
  EXPECT_EQ("b", string_file.string());
  EXPECT_EQ(1, string_file.Seek(0, SEEK_CUR));
  EXPECT_EQ(0, string_file.Seek(0, SEEK_SET));
  EXPECT_EQ(1, string_file.Read(&c, 1));
  EXPECT_EQ('b', c);
  EXPECT_EQ(1, string_file.Seek(0, SEEK_CUR));
  EXPECT_EQ(0, string_file.Read(&c, 1));
  EXPECT_EQ('b', c);
  EXPECT_EQ(1, string_file.Seek(0, SEEK_CUR));
  EXPECT_EQ("b", string_file.string());

  EXPECT_EQ(0, string_file.Seek(0, SEEK_SET));
  EXPECT_TRUE(string_file.Write("\0", 1));
  EXPECT_EQ(1u, string_file.string().size());
  EXPECT_EQ('\0', string_file.string()[0]);
  EXPECT_EQ(1, string_file.Seek(0, SEEK_CUR));
  EXPECT_EQ(1u, string_file.string().size());
  EXPECT_EQ('\0', string_file.string()[0]);
}

TEST(StringFile, SetString) {
  char kString1[] = "Four score";
  StringFile string_file;
  string_file.SetString(kString1);
  EXPECT_EQ(0, string_file.Seek(0, SEEK_SET));
  char buf[5] = "****";
  EXPECT_EQ(4, string_file.Read(buf, 4));
  EXPECT_STREQ("Four", buf);
  EXPECT_EQ(4, string_file.Seek(0, SEEK_CUR));
  EXPECT_EQ(static_cast<FileOffset>(strlen(kString1)),
            string_file.Seek(0, SEEK_END));
  EXPECT_EQ(kString1, string_file.string());

  char kString2[] = "and seven years ago";
  EXPECT_EQ(4, string_file.Seek(4, SEEK_SET));
  EXPECT_EQ(4, string_file.Seek(0, SEEK_CUR));
  string_file.SetString(kString2);
  EXPECT_EQ(0, string_file.Seek(0, SEEK_CUR));
  EXPECT_EQ(4, string_file.Read(buf, 4));
  EXPECT_STREQ("and ", buf);
  EXPECT_EQ(static_cast<FileOffset>(strlen(kString2)),
            string_file.Seek(0, SEEK_END));
  EXPECT_EQ(kString2, string_file.string());

  char kString3[] = "our fathers";
  EXPECT_EQ(4, string_file.Seek(4, SEEK_SET));
  EXPECT_EQ(4, string_file.Seek(0, SEEK_CUR));
  string_file.SetString(kString3);
  EXPECT_EQ(0, string_file.Seek(0, SEEK_CUR));
  EXPECT_EQ(4, string_file.Read(buf, 4));
  EXPECT_STREQ("our ", buf);
  EXPECT_EQ(static_cast<FileOffset>(strlen(kString3)),
            string_file.Seek(0, SEEK_END));
  EXPECT_EQ(kString3, string_file.string());
}

TEST(StringFile, ReadExactly) {
  StringFile string_file;
  string_file.SetString("1234567");
  char buf[4] = "***";
  EXPECT_TRUE(string_file.ReadExactly(buf, 3));
  EXPECT_STREQ("123", buf);
  EXPECT_TRUE(string_file.ReadExactly(buf, 3));
  EXPECT_STREQ("456", buf);
  EXPECT_FALSE(string_file.ReadExactly(buf, 3));
}

TEST(StringFile, Reset) {
  StringFile string_file;

  EXPECT_TRUE(string_file.Write("abc", 3));
  EXPECT_EQ(3u, string_file.string().size());
  EXPECT_EQ("abc", string_file.string());
  EXPECT_EQ(3, string_file.Seek(0, SEEK_CUR));
  char buf[10] = "*********";
  EXPECT_EQ(0, string_file.Seek(0, SEEK_SET));
  EXPECT_EQ(3, string_file.Read(&buf, 10));
  EXPECT_STREQ("abc******", buf);
  EXPECT_EQ(3, string_file.Seek(0, SEEK_CUR));
  EXPECT_FALSE(string_file.string().empty());

  string_file.Reset();
  EXPECT_TRUE(string_file.string().empty());
  EXPECT_EQ(0, string_file.Seek(0, SEEK_CUR));

  EXPECT_TRUE(string_file.Write("de", 2));
  EXPECT_EQ(2u, string_file.string().size());
  EXPECT_EQ("de", string_file.string());
  EXPECT_EQ(2, string_file.Seek(0, SEEK_CUR));
  EXPECT_EQ(0, string_file.Seek(0, SEEK_SET));
  EXPECT_EQ(2, string_file.Read(&buf, 10));
  EXPECT_STREQ("dec******", buf);
  EXPECT_EQ(2, string_file.Seek(0, SEEK_CUR));
  EXPECT_FALSE(string_file.string().empty());

  string_file.Reset();
  EXPECT_TRUE(string_file.string().empty());
  EXPECT_EQ(0, string_file.Seek(0, SEEK_CUR));

  EXPECT_TRUE(string_file.Write("fghi", 4));
  EXPECT_EQ(4u, string_file.string().size());
  EXPECT_EQ("fghi", string_file.string());
  EXPECT_EQ(4, string_file.Seek(0, SEEK_CUR));
  EXPECT_EQ(0, string_file.Seek(0, SEEK_SET));
  EXPECT_EQ(2, string_file.Read(&buf, 2));
  EXPECT_STREQ("fgc******", buf);
  EXPECT_EQ(2, string_file.Seek(0, SEEK_CUR));
  EXPECT_EQ(2, string_file.Read(&buf, 2));
  EXPECT_STREQ("hic******", buf);
  EXPECT_EQ(4, string_file.Seek(0, SEEK_CUR));
  EXPECT_FALSE(string_file.string().empty());

  string_file.Reset();
  EXPECT_TRUE(string_file.string().empty());
  EXPECT_EQ(0, string_file.Seek(0, SEEK_CUR));

  // Test resetting after a sparse write.
  EXPECT_EQ(1, string_file.Seek(1, SEEK_SET));
  EXPECT_TRUE(string_file.Write("j", 1));
  EXPECT_EQ(2u, string_file.string().size());
  EXPECT_EQ(std::string("\0j", 2), string_file.string());
  EXPECT_EQ(2, string_file.Seek(0, SEEK_CUR));
  EXPECT_FALSE(string_file.string().empty());

  string_file.Reset();
  EXPECT_TRUE(string_file.string().empty());
  EXPECT_EQ(0, string_file.Seek(0, SEEK_CUR));
}

TEST(StringFile, WriteInvalid) {
  StringFile string_file;

  EXPECT_EQ(0, string_file.Seek(0, SEEK_CUR));

  EXPECT_FALSE(string_file.Write(
      "",
      implicit_cast<size_t>(std::numeric_limits<FileOperationResult>::max()) +
          1));
  EXPECT_TRUE(string_file.string().empty());
  EXPECT_EQ(0, string_file.Seek(0, SEEK_CUR));

  EXPECT_TRUE(string_file.Write("a", 1));
  EXPECT_FALSE(string_file.Write(
      "",
      implicit_cast<size_t>(std::numeric_limits<FileOperationResult>::max())));
  EXPECT_EQ(1u, string_file.string().size());
  EXPECT_EQ("a", string_file.string());
  EXPECT_EQ(1, string_file.Seek(0, SEEK_CUR));
}

TEST(StringFile, WriteIoVec) {
  StringFile string_file;

  std::vector<WritableIoVec> iovecs;
  WritableIoVec iov;
  iov.iov_base = "";
  iov.iov_len = 0;
  iovecs.push_back(iov);
  EXPECT_TRUE(string_file.WriteIoVec(&iovecs));
  EXPECT_TRUE(string_file.string().empty());
  EXPECT_EQ(0, string_file.Seek(0, SEEK_CUR));

  iovecs.clear();
  iov.iov_base = "a";
  iov.iov_len = 1;
  iovecs.push_back(iov);
  EXPECT_TRUE(string_file.WriteIoVec(&iovecs));
  EXPECT_EQ(1u, string_file.string().size());
  EXPECT_EQ("a", string_file.string());
  EXPECT_EQ(1, string_file.Seek(0, SEEK_CUR));

  iovecs.clear();
  iovecs.push_back(iov);
  EXPECT_TRUE(string_file.WriteIoVec(&iovecs));
  EXPECT_EQ(2u, string_file.string().size());
  EXPECT_EQ("aa", string_file.string());
  EXPECT_EQ(2, string_file.Seek(0, SEEK_CUR));

  iovecs.clear();
  iovecs.push_back(iov);
  iov.iov_base = "bc";
  iov.iov_len = 2;
  iovecs.push_back(iov);
  EXPECT_TRUE(string_file.WriteIoVec(&iovecs));
  EXPECT_EQ(5u, string_file.string().size());
  EXPECT_EQ("aaabc", string_file.string());
  EXPECT_EQ(5, string_file.Seek(0, SEEK_CUR));

  EXPECT_TRUE(string_file.Write("def", 3));
  EXPECT_EQ(8u, string_file.string().size());
  EXPECT_EQ("aaabcdef", string_file.string());
  EXPECT_EQ(8, string_file.Seek(0, SEEK_CUR));

  iovecs.clear();
  iov.iov_base = "ghij";
  iov.iov_len = 4;
  iovecs.push_back(iov);
  iov.iov_base = "klmno";
  iov.iov_len = 5;
  iovecs.push_back(iov);
  EXPECT_TRUE(string_file.WriteIoVec(&iovecs));
  EXPECT_EQ(17u, string_file.string().size());
  EXPECT_EQ("aaabcdefghijklmno", string_file.string());
  EXPECT_EQ(17, string_file.Seek(0, SEEK_CUR));

  string_file.Reset();
  EXPECT_TRUE(string_file.string().empty());
  EXPECT_EQ(0, string_file.Seek(0, SEEK_CUR));

  iovecs.clear();
  iov.iov_base = "abcd";
  iov.iov_len = 4;
  iovecs.resize(16, iov);
  EXPECT_TRUE(string_file.WriteIoVec(&iovecs));
  EXPECT_EQ(64u, string_file.string().size());
  EXPECT_EQ("abcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
            string_file.string());
  EXPECT_EQ(64, string_file.Seek(0, SEEK_CUR));
}

TEST(StringFile, WriteIoVecInvalid) {
  StringFile string_file;

  std::vector<WritableIoVec> iovecs;
  EXPECT_FALSE(string_file.WriteIoVec(&iovecs));
  EXPECT_TRUE(string_file.string().empty());
  EXPECT_EQ(0, string_file.Seek(0, SEEK_CUR));

  WritableIoVec iov;
  EXPECT_EQ(1, string_file.Seek(1, SEEK_CUR));
  iov.iov_base = "a";
  iov.iov_len = std::numeric_limits<FileOperationResult>::max();
  iovecs.push_back(iov);
  EXPECT_FALSE(string_file.WriteIoVec(&iovecs));
  EXPECT_TRUE(string_file.string().empty());
  EXPECT_EQ(1, string_file.Seek(0, SEEK_CUR));

  iovecs.clear();
  iov.iov_base = "a";
  iov.iov_len = 1;
  iovecs.push_back(iov);
  iov.iov_len = std::numeric_limits<FileOperationResult>::max() - 1;
  iovecs.push_back(iov);
  EXPECT_FALSE(string_file.WriteIoVec(&iovecs));
  EXPECT_TRUE(string_file.string().empty());
  EXPECT_EQ(1, string_file.Seek(0, SEEK_CUR));
}

TEST(StringFile, Seek) {
  StringFile string_file;

  EXPECT_TRUE(string_file.Write("abcd", 4));
  EXPECT_EQ(4u, string_file.string().size());
  EXPECT_EQ("abcd", string_file.string());
  EXPECT_EQ(4, string_file.Seek(0, SEEK_CUR));

  EXPECT_EQ(0, string_file.Seek(0, SEEK_SET));
  EXPECT_TRUE(string_file.Write("efgh", 4));
  EXPECT_EQ(4u, string_file.string().size());
  EXPECT_EQ("efgh", string_file.string());
  EXPECT_EQ(4, string_file.Seek(0, SEEK_CUR));

  EXPECT_EQ(0, string_file.Seek(0, SEEK_SET));
  EXPECT_TRUE(string_file.Write("ijk", 3));
  EXPECT_EQ(4u, string_file.string().size());
  EXPECT_EQ("ijkh", string_file.string());
  EXPECT_EQ(3, string_file.Seek(0, SEEK_CUR));

  EXPECT_EQ(0, string_file.Seek(0, SEEK_SET));
  EXPECT_TRUE(string_file.Write("lmnop", 5));
  EXPECT_EQ(5u, string_file.string().size());
  EXPECT_EQ("lmnop", string_file.string());
  EXPECT_EQ(5, string_file.Seek(0, SEEK_CUR));

  EXPECT_EQ(1, string_file.Seek(1, SEEK_SET));
  EXPECT_TRUE(string_file.Write("q", 1));
  EXPECT_EQ(5u, string_file.string().size());
  EXPECT_EQ("lqnop", string_file.string());
  EXPECT_EQ(2, string_file.Seek(0, SEEK_CUR));

  EXPECT_EQ(1, string_file.Seek(-1, SEEK_CUR));
  EXPECT_TRUE(string_file.Write("r", 1));
  EXPECT_EQ(5u, string_file.string().size());
  EXPECT_EQ("lrnop", string_file.string());
  EXPECT_EQ(2, string_file.Seek(0, SEEK_CUR));

  EXPECT_TRUE(string_file.Write("s", 1));
  EXPECT_EQ(5u, string_file.string().size());
  EXPECT_EQ("lrsop", string_file.string());
  EXPECT_EQ(3, string_file.Seek(0, SEEK_CUR));

  EXPECT_EQ(2, string_file.Seek(-1, SEEK_CUR));
  EXPECT_TRUE(string_file.Write("t", 1));
  EXPECT_EQ(5u, string_file.string().size());
  EXPECT_EQ("lrtop", string_file.string());
  EXPECT_EQ(3, string_file.Seek(0, SEEK_CUR));

  EXPECT_EQ(4, string_file.Seek(-1, SEEK_END));
  EXPECT_TRUE(string_file.Write("u", 1));
  EXPECT_EQ(5u, string_file.string().size());
  EXPECT_EQ("lrtou", string_file.string());
  EXPECT_EQ(5, string_file.Seek(0, SEEK_CUR));

  EXPECT_EQ(0, string_file.Seek(-5, SEEK_END));
  EXPECT_TRUE(string_file.Write("v", 1));
  EXPECT_EQ(5u, string_file.string().size());
  EXPECT_EQ("vrtou", string_file.string());
  EXPECT_EQ(1, string_file.Seek(0, SEEK_CUR));

  EXPECT_EQ(5, string_file.Seek(0, SEEK_END));
  EXPECT_TRUE(string_file.Write("w", 1));
  EXPECT_EQ(6u, string_file.string().size());
  EXPECT_EQ("vrtouw", string_file.string());
  EXPECT_EQ(6, string_file.Seek(0, SEEK_CUR));

  EXPECT_EQ(8, string_file.Seek(2, SEEK_END));
  EXPECT_EQ(6u, string_file.string().size());
  EXPECT_EQ("vrtouw", string_file.string());

  EXPECT_EQ(6, string_file.Seek(0, SEEK_END));
  EXPECT_TRUE(string_file.Write("x", 1));
  EXPECT_EQ(7u, string_file.string().size());
  EXPECT_EQ("vrtouwx", string_file.string());
  EXPECT_EQ(7, string_file.Seek(0, SEEK_CUR));
}

TEST(StringFile, SeekSparse) {
  StringFile string_file;

  EXPECT_EQ(3, string_file.Seek(3, SEEK_SET));
  EXPECT_TRUE(string_file.string().empty());
  EXPECT_EQ(3, string_file.Seek(0, SEEK_CUR));

  EXPECT_TRUE(string_file.Write("abc", 3));
  EXPECT_EQ(6u, string_file.string().size());
  EXPECT_EQ(std::string("\0\0\0abc", 6), string_file.string());
  EXPECT_EQ(6, string_file.Seek(0, SEEK_CUR));

  EXPECT_EQ(9, string_file.Seek(3, SEEK_END));
  EXPECT_EQ(6u, string_file.string().size());
  EXPECT_EQ(9, string_file.Seek(0, SEEK_CUR));
  char c;
  EXPECT_EQ(0, string_file.Read(&c, 1));
  EXPECT_EQ(9, string_file.Seek(0, SEEK_CUR));
  EXPECT_EQ(6u, string_file.string().size());
  EXPECT_TRUE(string_file.Write("def", 3));
  EXPECT_EQ(12u, string_file.string().size());
  EXPECT_EQ(std::string("\0\0\0abc\0\0\0def", 12), string_file.string());
  EXPECT_EQ(12, string_file.Seek(0, SEEK_CUR));

  EXPECT_EQ(7, string_file.Seek(-5, SEEK_END));
  EXPECT_EQ(12u, string_file.string().size());
  EXPECT_EQ(7, string_file.Seek(0, SEEK_CUR));
  EXPECT_TRUE(string_file.Write("g", 1));
  EXPECT_EQ(12u, string_file.string().size());
  EXPECT_EQ(std::string("\0\0\0abc\0g\0def", 12), string_file.string());
  EXPECT_EQ(8, string_file.Seek(0, SEEK_CUR));

  EXPECT_EQ(15, string_file.Seek(7, SEEK_CUR));
  EXPECT_EQ(12u, string_file.string().size());
  EXPECT_EQ(15, string_file.Seek(0, SEEK_CUR));
  EXPECT_TRUE(string_file.Write("hij", 3));
  EXPECT_EQ(18u, string_file.string().size());
  EXPECT_EQ(std::string("\0\0\0abc\0g\0def\0\0\0hij", 18),
            string_file.string());
  EXPECT_EQ(18, string_file.Seek(0, SEEK_CUR));

  EXPECT_EQ(1, string_file.Seek(-17, SEEK_CUR));
  EXPECT_EQ(18u, string_file.string().size());
  EXPECT_EQ(1, string_file.Seek(0, SEEK_CUR));
  EXPECT_TRUE(string_file.Write("k", 1));
  EXPECT_EQ(18u, string_file.string().size());
  EXPECT_EQ(std::string("\0k\0abc\0g\0def\0\0\0hij", 18), string_file.string());
  EXPECT_EQ(2, string_file.Seek(0, SEEK_CUR));

  EXPECT_TRUE(string_file.Write("l", 1));
  EXPECT_TRUE(string_file.Write("mnop", 4));
  EXPECT_EQ(18u, string_file.string().size());
  EXPECT_EQ(std::string("\0klmnopg\0def\0\0\0hij", 18), string_file.string());
  EXPECT_EQ(7, string_file.Seek(0, SEEK_CUR));
}

TEST(StringFile, SeekInvalid) {
  StringFile string_file;

  EXPECT_EQ(0, string_file.Seek(0, SEEK_CUR));
  EXPECT_EQ(1, string_file.Seek(1, SEEK_SET));
  EXPECT_EQ(1, string_file.Seek(0, SEEK_CUR));
  EXPECT_LT(string_file.Seek(-1, SEEK_SET), 0);
  EXPECT_EQ(1, string_file.Seek(0, SEEK_CUR));
  EXPECT_LT(string_file.Seek(std::numeric_limits<FileOperationResult>::min(),
                             SEEK_SET),
            0);
  EXPECT_EQ(1, string_file.Seek(0, SEEK_CUR));
  EXPECT_LT(string_file.Seek(std::numeric_limits<FileOffset>::min(), SEEK_SET),
            0);
  EXPECT_EQ(1, string_file.Seek(0, SEEK_CUR));
  EXPECT_TRUE(string_file.string().empty());

  static_assert(SEEK_SET != 3 && SEEK_CUR != 3 && SEEK_END != 3,
                "3 must be invalid for whence");
  EXPECT_LT(string_file.Seek(0, 3), 0);

  string_file.Reset();
  EXPECT_EQ(0, string_file.Seek(0, SEEK_CUR));
  EXPECT_TRUE(string_file.string().empty());

  const FileOffset kMaxOffset = static_cast<FileOffset>(
      std::min(implicit_cast<uint64_t>(std::numeric_limits<FileOffset>::max()),
               implicit_cast<uint64_t>(std::numeric_limits<size_t>::max())));

  EXPECT_EQ(kMaxOffset, string_file.Seek(kMaxOffset, SEEK_SET));
  EXPECT_EQ(kMaxOffset, string_file.Seek(0, SEEK_CUR));
  EXPECT_LT(string_file.Seek(1, SEEK_CUR), 0);

  EXPECT_EQ(1, string_file.Seek(1, SEEK_SET));
  EXPECT_EQ(1, string_file.Seek(0, SEEK_CUR));
  EXPECT_LT(string_file.Seek(kMaxOffset, SEEK_CUR), 0);
}

TEST(StringFile, SeekSet) {
  StringFile string_file;
  EXPECT_TRUE(string_file.SeekSet(1));
  EXPECT_EQ(1, string_file.Seek(0, SEEK_CUR));
  EXPECT_TRUE(string_file.SeekSet(0));
  EXPECT_EQ(0, string_file.Seek(0, SEEK_CUR));
  EXPECT_TRUE(string_file.SeekSet(10));
  EXPECT_EQ(10, string_file.Seek(0, SEEK_CUR));
  EXPECT_FALSE(string_file.SeekSet(-1));
  EXPECT_EQ(10, string_file.Seek(0, SEEK_CUR));
  EXPECT_FALSE(
      string_file.SeekSet(std::numeric_limits<FileOperationResult>::min()));
  EXPECT_EQ(10, string_file.Seek(0, SEEK_CUR));
  EXPECT_FALSE(string_file.SeekSet(std::numeric_limits<FileOffset>::min()));
  EXPECT_EQ(10, string_file.Seek(0, SEEK_CUR));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
