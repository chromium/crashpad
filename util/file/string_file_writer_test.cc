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

#include "util/file/string_file_writer.h"

#include <algorithm>
#include <limits>

#include "gtest/gtest.h"

namespace {

using namespace crashpad;

TEST(StringFileWriter, EmptyFile) {
  StringFileWriter writer;
  EXPECT_TRUE(writer.string().empty());
  EXPECT_EQ(0, writer.Seek(0, SEEK_CUR));
  EXPECT_TRUE(writer.Write("", 0));
  EXPECT_TRUE(writer.string().empty());
  EXPECT_EQ(0, writer.Seek(0, SEEK_CUR));
}

TEST(StringFileWriter, OneByteFile) {
  StringFileWriter writer;

  EXPECT_TRUE(writer.Write("a", 1));
  EXPECT_EQ(1u, writer.string().size());
  EXPECT_EQ("a", writer.string());
  EXPECT_EQ(1, writer.Seek(0, SEEK_CUR));

  EXPECT_EQ(0, writer.Seek(0, SEEK_SET));
  EXPECT_TRUE(writer.Write("b", 1));
  EXPECT_EQ(1u, writer.string().size());
  EXPECT_EQ("b", writer.string());
  EXPECT_EQ(1, writer.Seek(0, SEEK_CUR));

  EXPECT_EQ(0, writer.Seek(0, SEEK_SET));
  EXPECT_TRUE(writer.Write("\0", 1));
  EXPECT_EQ(1u, writer.string().size());
  EXPECT_EQ('\0', writer.string()[0]);
  EXPECT_EQ(1, writer.Seek(0, SEEK_CUR));
}

TEST(StringFileWriter, Reset) {
  StringFileWriter writer;

  EXPECT_TRUE(writer.Write("abc", 3));
  EXPECT_EQ(3u, writer.string().size());
  EXPECT_EQ("abc", writer.string());
  EXPECT_EQ(3, writer.Seek(0, SEEK_CUR));

  writer.Reset();
  EXPECT_TRUE(writer.string().empty());
  EXPECT_EQ(0, writer.Seek(0, SEEK_CUR));

  EXPECT_TRUE(writer.Write("de", 2));
  EXPECT_EQ(2u, writer.string().size());
  EXPECT_EQ("de", writer.string());
  EXPECT_EQ(2, writer.Seek(0, SEEK_CUR));

  writer.Reset();
  EXPECT_TRUE(writer.string().empty());
  EXPECT_EQ(0, writer.Seek(0, SEEK_CUR));

  EXPECT_TRUE(writer.Write("fghi", 4));
  EXPECT_EQ(4u, writer.string().size());
  EXPECT_EQ("fghi", writer.string());
  EXPECT_EQ(4, writer.Seek(0, SEEK_CUR));

  writer.Reset();
  EXPECT_TRUE(writer.string().empty());
  EXPECT_EQ(0, writer.Seek(0, SEEK_CUR));

  // Test resetting after a sparse write.
  EXPECT_EQ(1, writer.Seek(1, SEEK_SET));
  EXPECT_TRUE(writer.Write("j", 1));
  EXPECT_EQ(2u, writer.string().size());
  EXPECT_EQ(std::string("\0j", 2), writer.string());
  EXPECT_EQ(2, writer.Seek(0, SEEK_CUR));

  writer.Reset();
  EXPECT_TRUE(writer.string().empty());
  EXPECT_EQ(0, writer.Seek(0, SEEK_CUR));
}

TEST(StringFileWriter, WriteInvalid) {
  StringFileWriter writer;

  EXPECT_EQ(0, writer.Seek(0, SEEK_CUR));

  EXPECT_FALSE(writer.Write(
      "", static_cast<size_t>(std::numeric_limits<ssize_t>::max()) + 1));
  EXPECT_TRUE(writer.string().empty());
  EXPECT_EQ(0, writer.Seek(0, SEEK_CUR));

  EXPECT_TRUE(writer.Write("a", 1));
  EXPECT_FALSE(writer.Write(
      "", static_cast<size_t>(std::numeric_limits<ssize_t>::max())));
  EXPECT_EQ(1u, writer.string().size());
  EXPECT_EQ("a", writer.string());
  EXPECT_EQ(1, writer.Seek(0, SEEK_CUR));
}

TEST(StringFileWriter, WriteIoVec) {
  StringFileWriter writer;

  std::vector<WritableIoVec> iovecs;
  WritableIoVec iov;
  iov.iov_base = "";
  iov.iov_len = 0;
  iovecs.push_back(iov);
  EXPECT_TRUE(writer.WriteIoVec(&iovecs));
  EXPECT_TRUE(writer.string().empty());
  EXPECT_EQ(0, writer.Seek(0, SEEK_CUR));

  iovecs.clear();
  iov.iov_base = "a";
  iov.iov_len = 1;
  iovecs.push_back(iov);
  EXPECT_TRUE(writer.WriteIoVec(&iovecs));
  EXPECT_EQ(1u, writer.string().size());
  EXPECT_EQ("a", writer.string());
  EXPECT_EQ(1, writer.Seek(0, SEEK_CUR));

  iovecs.clear();
  iovecs.push_back(iov);
  EXPECT_TRUE(writer.WriteIoVec(&iovecs));
  EXPECT_EQ(2u, writer.string().size());
  EXPECT_EQ("aa", writer.string());
  EXPECT_EQ(2, writer.Seek(0, SEEK_CUR));

  iovecs.clear();
  iovecs.push_back(iov);
  iov.iov_base = "bc";
  iov.iov_len = 2;
  iovecs.push_back(iov);
  EXPECT_TRUE(writer.WriteIoVec(&iovecs));
  EXPECT_EQ(5u, writer.string().size());
  EXPECT_EQ("aaabc", writer.string());
  EXPECT_EQ(5, writer.Seek(0, SEEK_CUR));

  EXPECT_TRUE(writer.Write("def", 3));
  EXPECT_EQ(8u, writer.string().size());
  EXPECT_EQ("aaabcdef", writer.string());
  EXPECT_EQ(8, writer.Seek(0, SEEK_CUR));

  iovecs.clear();
  iov.iov_base = "ghij";
  iov.iov_len = 4;
  iovecs.push_back(iov);
  iov.iov_base = "klmno";
  iov.iov_len = 5;
  iovecs.push_back(iov);
  EXPECT_TRUE(writer.WriteIoVec(&iovecs));
  EXPECT_EQ(17u, writer.string().size());
  EXPECT_EQ("aaabcdefghijklmno", writer.string());
  EXPECT_EQ(17, writer.Seek(0, SEEK_CUR));

  writer.Reset();
  EXPECT_TRUE(writer.string().empty());
  EXPECT_EQ(0, writer.Seek(0, SEEK_CUR));

  iovecs.clear();
  iov.iov_base = "abcd";
  iov.iov_len = 4;
  iovecs.resize(16, iov);
  EXPECT_TRUE(writer.WriteIoVec(&iovecs));
  EXPECT_EQ(64u, writer.string().size());
  EXPECT_EQ("abcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcdabcd",
            writer.string());
  EXPECT_EQ(64, writer.Seek(0, SEEK_CUR));
}

TEST(StringFileWriter, WriteIoVecInvalid) {
  StringFileWriter writer;

  std::vector<WritableIoVec> iovecs;
  EXPECT_FALSE(writer.WriteIoVec(&iovecs));
  EXPECT_TRUE(writer.string().empty());
  EXPECT_EQ(0, writer.Seek(0, SEEK_CUR));

  WritableIoVec iov;
  EXPECT_EQ(1, writer.Seek(1, SEEK_CUR));
  iov.iov_base = "a";
  iov.iov_len = std::numeric_limits<ssize_t>::max();
  iovecs.push_back(iov);
  EXPECT_FALSE(writer.WriteIoVec(&iovecs));
  EXPECT_TRUE(writer.string().empty());
  EXPECT_EQ(1, writer.Seek(0, SEEK_CUR));

  iovecs.clear();
  iov.iov_base = "a";
  iov.iov_len = 1;
  iovecs.push_back(iov);
  iov.iov_len = std::numeric_limits<ssize_t>::max() - 1;
  iovecs.push_back(iov);
  EXPECT_FALSE(writer.WriteIoVec(&iovecs));
  EXPECT_TRUE(writer.string().empty());
  EXPECT_EQ(1, writer.Seek(0, SEEK_CUR));
}

TEST(StringFileWriter, Seek) {
  StringFileWriter writer;

  EXPECT_TRUE(writer.Write("abcd", 4));
  EXPECT_EQ(4u, writer.string().size());
  EXPECT_EQ("abcd", writer.string());
  EXPECT_EQ(4, writer.Seek(0, SEEK_CUR));

  EXPECT_EQ(0, writer.Seek(0, SEEK_SET));
  EXPECT_TRUE(writer.Write("efgh", 4));
  EXPECT_EQ(4u, writer.string().size());
  EXPECT_EQ("efgh", writer.string());
  EXPECT_EQ(4, writer.Seek(0, SEEK_CUR));

  EXPECT_EQ(0, writer.Seek(0, SEEK_SET));
  EXPECT_TRUE(writer.Write("ijk", 3));
  EXPECT_EQ(4u, writer.string().size());
  EXPECT_EQ("ijkh", writer.string());
  EXPECT_EQ(3, writer.Seek(0, SEEK_CUR));

  EXPECT_EQ(0, writer.Seek(0, SEEK_SET));
  EXPECT_TRUE(writer.Write("lmnop", 5));
  EXPECT_EQ(5u, writer.string().size());
  EXPECT_EQ("lmnop", writer.string());
  EXPECT_EQ(5, writer.Seek(0, SEEK_CUR));

  EXPECT_EQ(1, writer.Seek(1, SEEK_SET));
  EXPECT_TRUE(writer.Write("q", 1));
  EXPECT_EQ(5u, writer.string().size());
  EXPECT_EQ("lqnop", writer.string());
  EXPECT_EQ(2, writer.Seek(0, SEEK_CUR));

  EXPECT_EQ(1, writer.Seek(-1, SEEK_CUR));
  EXPECT_TRUE(writer.Write("r", 1));
  EXPECT_EQ(5u, writer.string().size());
  EXPECT_EQ("lrnop", writer.string());
  EXPECT_EQ(2, writer.Seek(0, SEEK_CUR));

  EXPECT_TRUE(writer.Write("s", 1));
  EXPECT_EQ(5u, writer.string().size());
  EXPECT_EQ("lrsop", writer.string());
  EXPECT_EQ(3, writer.Seek(0, SEEK_CUR));

  EXPECT_EQ(2, writer.Seek(-1, SEEK_CUR));
  EXPECT_TRUE(writer.Write("t", 1));
  EXPECT_EQ(5u, writer.string().size());
  EXPECT_EQ("lrtop", writer.string());
  EXPECT_EQ(3, writer.Seek(0, SEEK_CUR));

  EXPECT_EQ(4, writer.Seek(-1, SEEK_END));
  EXPECT_TRUE(writer.Write("u", 1));
  EXPECT_EQ(5u, writer.string().size());
  EXPECT_EQ("lrtou", writer.string());
  EXPECT_EQ(5, writer.Seek(0, SEEK_CUR));

  EXPECT_EQ(0, writer.Seek(-5, SEEK_END));
  EXPECT_TRUE(writer.Write("v", 1));
  EXPECT_EQ(5u, writer.string().size());
  EXPECT_EQ("vrtou", writer.string());
  EXPECT_EQ(1, writer.Seek(0, SEEK_CUR));

  EXPECT_EQ(5, writer.Seek(0, SEEK_END));
  EXPECT_TRUE(writer.Write("w", 1));
  EXPECT_EQ(6u, writer.string().size());
  EXPECT_EQ("vrtouw", writer.string());
  EXPECT_EQ(6, writer.Seek(0, SEEK_CUR));

  EXPECT_EQ(8, writer.Seek(2, SEEK_END));
  EXPECT_EQ(6u, writer.string().size());
  EXPECT_EQ("vrtouw", writer.string());

  EXPECT_EQ(6, writer.Seek(0, SEEK_END));
  EXPECT_TRUE(writer.Write("x", 1));
  EXPECT_EQ(7u, writer.string().size());
  EXPECT_EQ("vrtouwx", writer.string());
  EXPECT_EQ(7, writer.Seek(0, SEEK_CUR));
}

TEST(StringFileWriter, SeekSparse) {
  StringFileWriter writer;

  EXPECT_EQ(3, writer.Seek(3, SEEK_SET));
  EXPECT_TRUE(writer.string().empty());
  EXPECT_EQ(3, writer.Seek(0, SEEK_CUR));

  EXPECT_TRUE(writer.Write("abc", 3));
  EXPECT_EQ(6u, writer.string().size());
  EXPECT_EQ(std::string("\0\0\0abc", 6), writer.string());
  EXPECT_EQ(6, writer.Seek(0, SEEK_CUR));

  EXPECT_EQ(9, writer.Seek(3, SEEK_END));
  EXPECT_EQ(6u, writer.string().size());
  EXPECT_EQ(9, writer.Seek(0, SEEK_CUR));
  EXPECT_TRUE(writer.Write("def", 3));
  EXPECT_EQ(12u, writer.string().size());
  EXPECT_EQ(std::string("\0\0\0abc\0\0\0def", 12), writer.string());
  EXPECT_EQ(12, writer.Seek(0, SEEK_CUR));

  EXPECT_EQ(7, writer.Seek(-5, SEEK_END));
  EXPECT_EQ(12u, writer.string().size());
  EXPECT_EQ(7, writer.Seek(0, SEEK_CUR));
  EXPECT_TRUE(writer.Write("g", 1));
  EXPECT_EQ(12u, writer.string().size());
  EXPECT_EQ(std::string("\0\0\0abc\0g\0def", 12), writer.string());
  EXPECT_EQ(8, writer.Seek(0, SEEK_CUR));

  EXPECT_EQ(15, writer.Seek(7, SEEK_CUR));
  EXPECT_EQ(12u, writer.string().size());
  EXPECT_EQ(15, writer.Seek(0, SEEK_CUR));
  EXPECT_TRUE(writer.Write("hij", 3));
  EXPECT_EQ(18u, writer.string().size());
  EXPECT_EQ(std::string("\0\0\0abc\0g\0def\0\0\0hij", 18), writer.string());
  EXPECT_EQ(18, writer.Seek(0, SEEK_CUR));

  EXPECT_EQ(1, writer.Seek(-17, SEEK_CUR));
  EXPECT_EQ(18u, writer.string().size());
  EXPECT_EQ(1, writer.Seek(0, SEEK_CUR));
  EXPECT_TRUE(writer.Write("k", 1));
  EXPECT_EQ(18u, writer.string().size());
  EXPECT_EQ(std::string("\0k\0abc\0g\0def\0\0\0hij", 18), writer.string());
  EXPECT_EQ(2, writer.Seek(0, SEEK_CUR));

  EXPECT_TRUE(writer.Write("l", 1));
  EXPECT_TRUE(writer.Write("mnop", 4));
  EXPECT_EQ(18u, writer.string().size());
  EXPECT_EQ(std::string("\0klmnopg\0def\0\0\0hij", 18), writer.string());
  EXPECT_EQ(7, writer.Seek(0, SEEK_CUR));
}

TEST(StringFileWriter, SeekInvalid) {
  StringFileWriter writer;

  EXPECT_EQ(0, writer.Seek(0, SEEK_CUR));
  EXPECT_EQ(1, writer.Seek(1, SEEK_SET));
  EXPECT_EQ(1, writer.Seek(0, SEEK_CUR));
  EXPECT_LT(writer.Seek(-1, SEEK_SET), 0);
  EXPECT_EQ(1, writer.Seek(0, SEEK_CUR));
  EXPECT_LT(writer.Seek(std::numeric_limits<ssize_t>::min(), SEEK_SET), 0);
  EXPECT_EQ(1, writer.Seek(0, SEEK_CUR));
  EXPECT_LT(writer.Seek(std::numeric_limits<off_t>::min(), SEEK_SET), 0);
  EXPECT_EQ(1, writer.Seek(0, SEEK_CUR));
  EXPECT_TRUE(writer.string().empty());

  COMPILE_ASSERT(SEEK_SET != 3 && SEEK_CUR != 3 && SEEK_END != 3,
                 three_must_be_invalid_for_whence);
  EXPECT_LT(writer.Seek(0, 3), 0);

  writer.Reset();
  EXPECT_EQ(0, writer.Seek(0, SEEK_CUR));
  EXPECT_TRUE(writer.string().empty());

  const off_t kMaxOffset =
      std::min(static_cast<uint64_t>(std::numeric_limits<off_t>::max()),
               static_cast<uint64_t>(std::numeric_limits<size_t>::max()));

  EXPECT_EQ(kMaxOffset, writer.Seek(kMaxOffset, SEEK_SET));
  EXPECT_EQ(kMaxOffset, writer.Seek(0, SEEK_CUR));
  EXPECT_LT(writer.Seek(1, SEEK_CUR), 0);

  EXPECT_EQ(1, writer.Seek(1, SEEK_SET));
  EXPECT_EQ(1, writer.Seek(0, SEEK_CUR));
  EXPECT_LT(writer.Seek(kMaxOffset, SEEK_CUR), 0);
}

}  // namespace
