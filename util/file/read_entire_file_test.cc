// Copyright 2016 The Crashpad Authors. All rights reserved.
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

#include "util/file/read_entire_file.h"

#include <limits>
#include <type_traits>

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "gtest/gtest.h"
#include "test/scoped_temp_dir.h"
#include "util/file/file_io.h"
#include "util/misc/implicit_cast.h"

namespace crashpad {
namespace test {
namespace {

class ReadEntireFileTest : public testing::Test {
 public:
  ReadEntireFileTest()
      : Test(),
        dir_(),
        path_(dir_.path().Append(FILE_PATH_LITERAL("file.txt"))) {}
  ~ReadEntireFileTest() {}

 protected:
  void WriteContents(const std::string& string) {
    ScopedFileHandle handle(
        LoggingOpenFileForWrite(path(),
                                FileWriteMode::kTruncateOrCreate,
                                FilePermissions::kWorldReadable));
    ASSERT_TRUE(handle.is_valid());
    ASSERT_TRUE(LoggingWriteFile(handle.get(), string.c_str(), string.size()));
  }

  const base::FilePath& path() const { return path_; }

 private:
  ScopedTempDir dir_;
  base::FilePath path_;
};

TEST_F(ReadEntireFileTest, ReadEntireFile_String) {
  std::string contents;

  EXPECT_EQ(ReadFileResult::kOpenError, ReadEntireFile(path(), &contents));

  WriteContents(std::string());
  EXPECT_EQ(ReadFileResult::kSuccess, ReadEntireFile(path(), &contents));
  EXPECT_TRUE(contents.empty());

  WriteContents(std::string("hello"));
  EXPECT_EQ(ReadFileResult::kSuccess, ReadEntireFile(path(), &contents));
  EXPECT_EQ("hello", contents);

  WriteContents(std::string("one line\n"));
  EXPECT_EQ(ReadFileResult::kSuccess, ReadEntireFile(path(), &contents));
  EXPECT_EQ("one line\n", contents);

  WriteContents(std::string("one line\ntwo\n"));
  EXPECT_EQ(ReadFileResult::kSuccess, ReadEntireFile(path(), &contents));
  EXPECT_EQ("one line\ntwo\n", contents);
}

TEST_F(ReadEntireFileTest, ReadEntireFile_Vector) {
  std::vector<std::string> lines;

  EXPECT_EQ(ReadFileResult::kOpenError, ReadEntireFile(path(), &lines));

  WriteContents(std::string());
  EXPECT_EQ(ReadFileResult::kSuccess, ReadEntireFile(path(), &lines));
  EXPECT_TRUE(lines.empty());

  WriteContents(std::string("one line\n"));
  EXPECT_EQ(ReadFileResult::kSuccess, ReadEntireFile(path(), &lines));
  EXPECT_EQ(1u, lines.size());
  EXPECT_EQ("one line", lines[0]);

  WriteContents(std::string("terminated\nunterminated"));
  EXPECT_EQ(ReadFileResult::kFormatError, ReadEntireFile(path(), &lines));
}

TEST_F(ReadEntireFileTest, ReadOneLineFile_String) {
  std::string line;

  EXPECT_EQ(ReadFileResult::kOpenError, ReadOneLineFile(path(), &line));

  WriteContents(std::string("one line\n"));
  EXPECT_EQ(ReadFileResult::kSuccess, ReadOneLineFile(path(), &line));
  EXPECT_EQ("one line", line);

  WriteContents(std::string("\n"));
  EXPECT_EQ(ReadFileResult::kSuccess, ReadOneLineFile(path(), &line));
  EXPECT_TRUE(line.empty());

  WriteContents(std::string());
  EXPECT_EQ(ReadFileResult::kFormatError, ReadOneLineFile(path(), &line));

  WriteContents(std::string("one line\ntwo\n"));
  EXPECT_EQ(ReadFileResult::kFormatError, ReadOneLineFile(path(), &line));

  WriteContents(std::string("unterminated"));
  EXPECT_EQ(ReadFileResult::kFormatError, ReadOneLineFile(path(), &line));
}

template <typename T>
class ReadNumericOneLineFileTest : public ReadEntireFileTest {
 public:
  ReadNumericOneLineFileTest() : ReadEntireFileTest() {}
  ~ReadNumericOneLineFileTest() {}
};

using ReadNumericOneLineFileTestTypes = testing::Types<char,
                                                       signed char,
                                                       unsigned char,
                                                       short,
                                                       unsigned short,
                                                       int,
                                                       unsigned int,
                                                       long,
                                                       unsigned long,
                                                       long long,
                                                       unsigned long long>;
TYPED_TEST_CASE(ReadNumericOneLineFileTest, ReadNumericOneLineFileTestTypes);

TYPED_TEST(ReadNumericOneLineFileTest, ReadOneLineFile) {
  TypeParam value;

  EXPECT_EQ(ReadFileResult::kOpenError, ReadOneLineFile(this->path(), &value));

  this->WriteContents(std::string("0\n"));
  EXPECT_EQ(ReadFileResult::kSuccess, ReadOneLineFile(this->path(), &value));
  EXPECT_EQ(static_cast<TypeParam>(0), value);

  this->WriteContents(std::string("1\n"));
  EXPECT_EQ(ReadFileResult::kSuccess, ReadOneLineFile(this->path(), &value));
  EXPECT_EQ(static_cast<TypeParam>(1), value);

  this->WriteContents(std::string("127\n"));
  EXPECT_EQ(ReadFileResult::kSuccess, ReadOneLineFile(this->path(), &value));
  EXPECT_EQ(static_cast<TypeParam>(127), value);

  if (std::is_signed<TypeParam>::value) {
    this->WriteContents(base::StringPrintf(
        "%lld\n",
        implicit_cast<long long>(std::numeric_limits<TypeParam>::min())));
    EXPECT_EQ(ReadFileResult::kSuccess, ReadOneLineFile(this->path(), &value));
    EXPECT_EQ(std::numeric_limits<TypeParam>::min(), value);

    this->WriteContents(base::StringPrintf(
        "%lld\n",
        implicit_cast<long long>(std::numeric_limits<TypeParam>::max())));
    EXPECT_EQ(ReadFileResult::kSuccess, ReadOneLineFile(this->path(), &value));
    EXPECT_EQ(std::numeric_limits<TypeParam>::max(), value);

    if (std::numeric_limits<TypeParam>::digits <
        std::numeric_limits<long long>::digits) {
      this->WriteContents(base::StringPrintf(
          "%lld\n",
          implicit_cast<long long>(std::numeric_limits<TypeParam>::min()) - 1));
      EXPECT_EQ(ReadFileResult::kFormatError,
                ReadOneLineFile(this->path(), &value));

      this->WriteContents(base::StringPrintf(
          "%lld\n",
          implicit_cast<long long>(std::numeric_limits<TypeParam>::max()) + 1));
      EXPECT_EQ(ReadFileResult::kFormatError,
                ReadOneLineFile(this->path(), &value));
    }
  } else if (std::is_unsigned<TypeParam>::value) {
    this->WriteContents(
        base::StringPrintf("%llu\n",
                           implicit_cast<unsigned long long>(
                               std::numeric_limits<TypeParam>::max())));
    EXPECT_EQ(ReadFileResult::kSuccess, ReadOneLineFile(this->path(), &value));
    EXPECT_EQ(std::numeric_limits<TypeParam>::max(), value);

    this->WriteContents(base::StringPrintf("-1\n"));
    EXPECT_EQ(ReadFileResult::kFormatError,
              ReadOneLineFile(this->path(), &value));

    if (std::numeric_limits<TypeParam>::digits <
        std::numeric_limits<unsigned long long>::digits) {
      this->WriteContents(
          base::StringPrintf("%lld\n",
                             implicit_cast<unsigned long long>(
                                 std::numeric_limits<TypeParam>::max()) +
                                 1));
      EXPECT_EQ(ReadFileResult::kFormatError,
                ReadOneLineFile(this->path(), &value));
    }
  } else {
    FAIL() << "neither signed nor unsigned";
  }

  this->WriteContents(std::string("\n"));
  EXPECT_EQ(ReadFileResult::kFormatError,
            ReadOneLineFile(this->path(), &value));

  this->WriteContents(std::string());
  EXPECT_EQ(ReadFileResult::kFormatError,
            ReadOneLineFile(this->path(), &value));

  this->WriteContents(std::string("1"));
  EXPECT_EQ(ReadFileResult::kFormatError,
            ReadOneLineFile(this->path(), &value));

  this->WriteContents(std::string("0\n1\n"));
  EXPECT_EQ(ReadFileResult::kFormatError,
            ReadOneLineFile(this->path(), &value));

  this->WriteContents(std::string("\n1\n"));
  EXPECT_EQ(ReadFileResult::kFormatError,
            ReadOneLineFile(this->path(), &value));

  this->WriteContents(std::string(" 1\n"));
  EXPECT_EQ(ReadFileResult::kFormatError,
            ReadOneLineFile(this->path(), &value));

  this->WriteContents(std::string("1 \n"));
  EXPECT_EQ(ReadFileResult::kFormatError,
            ReadOneLineFile(this->path(), &value));

  this->WriteContents(std::string("string\n"));
  EXPECT_EQ(ReadFileResult::kFormatError,
            ReadOneLineFile(this->path(), &value));
}

}  // namespace
}  // namespace test
}  // namespace crashpad
