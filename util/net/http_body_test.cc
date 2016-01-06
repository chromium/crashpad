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

#include "util/net/http_body.h"

#include <string.h>

#include "gtest/gtest.h"
#include "test/paths.h"
#include "util/misc/implicit_cast.h"
#include "util/net/http_body_test_util.h"

namespace crashpad {
namespace test {
namespace {

void ExpectBufferSet(const uint8_t* actual,
                     uint8_t expected_byte,
                     size_t num_expected_bytes) {
  for (size_t i = 0; i < num_expected_bytes; ++i) {
    EXPECT_EQ(expected_byte, actual[i]) << i;
  }
}

TEST(StringHTTPBodyStream, EmptyString) {
  uint8_t buf[32];
  memset(buf, '!', sizeof(buf));

  std::string empty_string;
  StringHTTPBodyStream stream(empty_string);
  EXPECT_EQ(0, stream.GetBytesBuffer(buf, sizeof(buf)));
  ExpectBufferSet(buf, '!', sizeof(buf));
}

TEST(StringHTTPBodyStream, SmallString) {
  uint8_t buf[32];
  memset(buf, '!', sizeof(buf));

  std::string string("Hello, world");
  StringHTTPBodyStream stream(string);
  EXPECT_EQ(implicit_cast<FileOperationResult>(string.length()),
            stream.GetBytesBuffer(buf, sizeof(buf)));

  std::string actual(reinterpret_cast<const char*>(buf), string.length());
  EXPECT_EQ(string, actual);
  ExpectBufferSet(buf + string.length(), '!', sizeof(buf) - string.length());

  EXPECT_EQ(0, stream.GetBytesBuffer(buf, sizeof(buf)));
}

TEST(StringHTTPBodyStream, MultipleReads) {
  uint8_t buf[2];
  memset(buf, '!', sizeof(buf));

  {
    std::string string("test");
    SCOPED_TRACE("aligned buffer boundary");

    StringHTTPBodyStream stream(string);
    EXPECT_EQ(2, stream.GetBytesBuffer(buf, sizeof(buf)));
    EXPECT_EQ('t', buf[0]);
    EXPECT_EQ('e', buf[1]);
    EXPECT_EQ(2, stream.GetBytesBuffer(buf, sizeof(buf)));
    EXPECT_EQ('s', buf[0]);
    EXPECT_EQ('t', buf[1]);
    EXPECT_EQ(0, stream.GetBytesBuffer(buf, sizeof(buf)));
    EXPECT_EQ('s', buf[0]);
    EXPECT_EQ('t', buf[1]);
  }

  {
    std::string string("abc");
    SCOPED_TRACE("unaligned buffer boundary");

    StringHTTPBodyStream stream(string);
    EXPECT_EQ(2, stream.GetBytesBuffer(buf, sizeof(buf)));
    EXPECT_EQ('a', buf[0]);
    EXPECT_EQ('b', buf[1]);
    EXPECT_EQ(1, stream.GetBytesBuffer(buf, sizeof(buf)));
    EXPECT_EQ('c', buf[0]);
    EXPECT_EQ('b', buf[1]);  // Unmodified from last read.
    EXPECT_EQ(0, stream.GetBytesBuffer(buf, sizeof(buf)));
    EXPECT_EQ('c', buf[0]);
    EXPECT_EQ('b', buf[1]);
  }
}

TEST(FileHTTPBodyStream, ReadASCIIFile) {
  base::FilePath path = Paths::TestDataRoot().Append(
      FILE_PATH_LITERAL("util/net/testdata/ascii_http_body.txt"));
  FileHTTPBodyStream stream(path);
  std::string contents = ReadStreamToString(&stream, 32);
  EXPECT_EQ("This is a test.\n", contents);

  // Make sure that the file is not read again after it has been read to
  // completion.
  uint8_t buf[8];
  memset(buf, '!', sizeof(buf));
  EXPECT_EQ(0, stream.GetBytesBuffer(buf, sizeof(buf)));
  ExpectBufferSet(buf, '!', sizeof(buf));
  EXPECT_EQ(0, stream.GetBytesBuffer(buf, sizeof(buf)));
  ExpectBufferSet(buf, '!', sizeof(buf));
}

TEST(FileHTTPBodyStream, ReadBinaryFile) {
  // HEX contents of file: |FEEDFACE A11A15|.
  base::FilePath path = Paths::TestDataRoot().Append(
      FILE_PATH_LITERAL("util/net/testdata/binary_http_body.dat"));
  // This buffer size was chosen so that reading the file takes multiple reads.
  uint8_t buf[4];

  FileHTTPBodyStream stream(path);

  memset(buf, '!', sizeof(buf));
  EXPECT_EQ(4, stream.GetBytesBuffer(buf, sizeof(buf)));
  EXPECT_EQ(0xfe, buf[0]);
  EXPECT_EQ(0xed, buf[1]);
  EXPECT_EQ(0xfa, buf[2]);
  EXPECT_EQ(0xce, buf[3]);

  memset(buf, '!', sizeof(buf));
  EXPECT_EQ(3, stream.GetBytesBuffer(buf, sizeof(buf)));
  EXPECT_EQ(0xa1, buf[0]);
  EXPECT_EQ(0x1a, buf[1]);
  EXPECT_EQ(0x15, buf[2]);
  EXPECT_EQ('!', buf[3]);

  memset(buf, '!', sizeof(buf));
  EXPECT_EQ(0, stream.GetBytesBuffer(buf, sizeof(buf)));
  ExpectBufferSet(buf, '!', sizeof(buf));
  EXPECT_EQ(0, stream.GetBytesBuffer(buf, sizeof(buf)));
  ExpectBufferSet(buf, '!', sizeof(buf));
}

TEST(FileHTTPBodyStream, NonExistentFile) {
  base::FilePath path = base::FilePath(
      FILE_PATH_LITERAL("/var/empty/crashpad/util/net/http_body/null"));
  FileHTTPBodyStream stream(path);

  uint8_t buf = 0xff;
  EXPECT_LT(stream.GetBytesBuffer(&buf, 1), 0);
  EXPECT_EQ(0xff, buf);
  EXPECT_LT(stream.GetBytesBuffer(&buf, 1), 0);
  EXPECT_EQ(0xff, buf);
}

TEST(CompositeHTTPBodyStream, TwoEmptyStrings) {
  std::vector<HTTPBodyStream*> parts;
  parts.push_back(new StringHTTPBodyStream(std::string()));
  parts.push_back(new StringHTTPBodyStream(std::string()));

  CompositeHTTPBodyStream stream(parts);

  uint8_t buf[5];
  memset(buf, '!', sizeof(buf));
  EXPECT_EQ(0, stream.GetBytesBuffer(buf, sizeof(buf)));
  ExpectBufferSet(buf, '!', sizeof(buf));
}

class CompositeHTTPBodyStreamBufferSize
    : public testing::TestWithParam<size_t> {
};

TEST_P(CompositeHTTPBodyStreamBufferSize, ThreeStringParts) {
  std::string string1("crashpad");
  std::string string2("test");
  std::string string3("foobar");
  const size_t all_strings_length =
      string1.length() + string2.length() + string3.length();
  std::string buf(all_strings_length + 3, '!');

  std::vector<HTTPBodyStream*> parts;
  parts.push_back(new StringHTTPBodyStream(string1));
  parts.push_back(new StringHTTPBodyStream(string2));
  parts.push_back(new StringHTTPBodyStream(string3));

  CompositeHTTPBodyStream stream(parts);

  std::string actual_string = ReadStreamToString(&stream, GetParam());
  EXPECT_EQ(string1 + string2 + string3, actual_string);

  ExpectBufferSet(reinterpret_cast<uint8_t*>(&buf[all_strings_length]), '!', 3);
}

TEST_P(CompositeHTTPBodyStreamBufferSize, StringsAndFile) {
  std::string string1("Hello! ");
  std::string string2(" Goodbye :)");

  std::vector<HTTPBodyStream*> parts;
  parts.push_back(new StringHTTPBodyStream(string1));
  base::FilePath path = Paths::TestDataRoot().Append(
      FILE_PATH_LITERAL("util/net/testdata/ascii_http_body.txt"));
  parts.push_back(new FileHTTPBodyStream(path));
  parts.push_back(new StringHTTPBodyStream(string2));

  CompositeHTTPBodyStream stream(parts);

  std::string expected_string = string1 + "This is a test.\n" + string2;
  std::string actual_string = ReadStreamToString(&stream, GetParam());
  EXPECT_EQ(expected_string, actual_string);
}

INSTANTIATE_TEST_CASE_P(VariableBufferSize,
                        CompositeHTTPBodyStreamBufferSize,
                        testing::Values(1, 2, 9, 16, 31, 128, 1024));

}  // namespace
}  // namespace test
}  // namespace crashpad
