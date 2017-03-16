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

#include "util/file/file_line_reader.h"

#include <vector>

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "gtest/gtest.h"
#include "util/file/string_file.h"

namespace crashpad {
namespace test {
namespace {

TEST(FileLineReader, EmptyFile) {
  StringFile string_file;
  FileLineReader file_line_reader(&string_file);

  std::string line;
  EXPECT_EQ(FileLineReader::Result::kEndOfFile,
            file_line_reader.GetLine(&line));

  // The file is still at EOF.
  EXPECT_EQ(FileLineReader::Result::kEndOfFile,
            file_line_reader.GetLine(&line));
}

TEST(FileLineReader, EmptyOneLineFile) {
  StringFile string_file;
  string_file.SetString("\n");
  FileLineReader file_line_reader(&string_file);

  std::string line;
  ASSERT_EQ(FileLineReader::Result::kSuccess, file_line_reader.GetLine(&line));
  EXPECT_EQ(string_file.string(), line);
  EXPECT_EQ(FileLineReader::Result::kEndOfFile,
            file_line_reader.GetLine(&line));

  // The file is still at EOF.
  EXPECT_EQ(FileLineReader::Result::kEndOfFile,
            file_line_reader.GetLine(&line));
}

TEST(FileLineReader, SmallOneLineFile) {
  StringFile string_file;
  string_file.SetString("one line\n");
  FileLineReader file_line_reader(&string_file);

  std::string line;
  ASSERT_EQ(FileLineReader::Result::kSuccess, file_line_reader.GetLine(&line));
  EXPECT_EQ(string_file.string(), line);
  EXPECT_EQ(FileLineReader::Result::kEndOfFile,
            file_line_reader.GetLine(&line));

  // The file is still at EOF.
  EXPECT_EQ(FileLineReader::Result::kEndOfFile,
            file_line_reader.GetLine(&line));
}

TEST(FileLineReader, SmallOneLineFileWithoutNewline) {
  StringFile string_file;
  string_file.SetString("no newline");
  FileLineReader file_line_reader(&string_file);

  std::string line;
  ASSERT_EQ(FileLineReader::Result::kSuccess, file_line_reader.GetLine(&line));
  EXPECT_EQ(string_file.string(), line);
  EXPECT_EQ(FileLineReader::Result::kEndOfFile,
            file_line_reader.GetLine(&line));

  // The file is still at EOF.
  EXPECT_EQ(FileLineReader::Result::kEndOfFile,
            file_line_reader.GetLine(&line));
}

TEST(FileLineReader, SmallMultiLineFile) {
  StringFile string_file;
  string_file.SetString("first\nsecond line\n3rd\n");
  FileLineReader file_line_reader(&string_file);

  std::string line;
  ASSERT_EQ(FileLineReader::Result::kSuccess, file_line_reader.GetLine(&line));
  EXPECT_EQ("first\n", line);
  ASSERT_EQ(FileLineReader::Result::kSuccess, file_line_reader.GetLine(&line));
  EXPECT_EQ("second line\n", line);
  ASSERT_EQ(FileLineReader::Result::kSuccess, file_line_reader.GetLine(&line));
  EXPECT_EQ("3rd\n", line);
  EXPECT_EQ(FileLineReader::Result::kEndOfFile,
            file_line_reader.GetLine(&line));

  // The file is still at EOF.
  EXPECT_EQ(FileLineReader::Result::kEndOfFile,
            file_line_reader.GetLine(&line));
}

TEST(FileLineReader, EmptyLineMultiLineFile) {
  StringFile string_file;
  string_file.SetString("first\n\n\n4444\n");
  FileLineReader file_line_reader(&string_file);

  std::string line;
  ASSERT_EQ(FileLineReader::Result::kSuccess, file_line_reader.GetLine(&line));
  EXPECT_EQ("first\n", line);
  ASSERT_EQ(FileLineReader::Result::kSuccess, file_line_reader.GetLine(&line));
  EXPECT_EQ("\n", line);
  ASSERT_EQ(FileLineReader::Result::kSuccess, file_line_reader.GetLine(&line));
  EXPECT_EQ("\n", line);
  ASSERT_EQ(FileLineReader::Result::kSuccess, file_line_reader.GetLine(&line));
  EXPECT_EQ("4444\n", line);
  EXPECT_EQ(FileLineReader::Result::kEndOfFile,
            file_line_reader.GetLine(&line));

  // The file is still at EOF.
  EXPECT_EQ(FileLineReader::Result::kEndOfFile,
            file_line_reader.GetLine(&line));
}

TEST(FileLineReader, LongOneLineFile) {
  std::string contents(50000, '!');
  contents[1] = '?';
  contents.push_back('\n');

  StringFile string_file;
  string_file.SetString(contents);
  FileLineReader file_line_reader(&string_file);

  std::string line;
  ASSERT_EQ(FileLineReader::Result::kSuccess, file_line_reader.GetLine(&line));
  EXPECT_EQ(contents, line);
  EXPECT_EQ(FileLineReader::Result::kEndOfFile,
            file_line_reader.GetLine(&line));

  // The file is still at EOF.
  EXPECT_EQ(FileLineReader::Result::kEndOfFile,
            file_line_reader.GetLine(&line));
}

void TestLongMultiLineFile(int base_length) {
  std::vector<std::string> lines;
  std::string contents;
  for (size_t line_index = 0; line_index <= 'z' - 'a'; ++line_index) {
    char c = 'a' + static_cast<char>(line_index);

    // Mix up the lengths a little.
    std::string line(base_length + line_index * ((line_index % 3) - 1), c);

    // Mix up the data a little too.
    ASSERT_LT(line_index, line.size());
    line[line_index] -= ('a' - 'A');

    line.push_back('\n');
    contents.append(line);
    lines.push_back(line);
  }

  StringFile string_file;
  string_file.SetString(contents);
  FileLineReader file_line_reader(&string_file);

  std::string line;
  for (size_t line_index = 0; line_index < lines.size(); ++line_index) {
    SCOPED_TRACE(base::StringPrintf("line_index %" PRIuS, line_index));
    ASSERT_EQ(FileLineReader::Result::kSuccess,
              file_line_reader.GetLine(&line));
    EXPECT_EQ(lines[line_index], line);
  }
  EXPECT_EQ(FileLineReader::Result::kEndOfFile,
            file_line_reader.GetLine(&line));

  // The file is still at EOF.
  EXPECT_EQ(FileLineReader::Result::kEndOfFile,
            file_line_reader.GetLine(&line));
}

TEST(FileLineReader, LongMultiLineFile) {
  TestLongMultiLineFile(500);
}

TEST(FileLineReader, ReallyLongMultiLineFile) {
  TestLongMultiLineFile(5000);
}

TEST(FileLineReader, EmbeddedNUL) {
  const char kString[] = "embedded\0NUL\n";
  StringFile string_file;
  string_file.SetString(std::string(kString, arraysize(kString) - 1));
  FileLineReader file_line_reader(&string_file);

  std::string line;
  ASSERT_EQ(FileLineReader::Result::kSuccess, file_line_reader.GetLine(&line));
  EXPECT_EQ(string_file.string(), line);
  EXPECT_EQ(FileLineReader::Result::kEndOfFile,
            file_line_reader.GetLine(&line));

  // The file is still at EOF.
  EXPECT_EQ(FileLineReader::Result::kEndOfFile,
            file_line_reader.GetLine(&line));
}

TEST(FileLineReader, EdgeCases) {
  const size_t kSizes[] = {4094, 4095, 4096, 4097, 8190, 8191, 8192, 8193};
  for (size_t index = 0; index < arraysize(kSizes); ++index) {
    size_t size = kSizes[index];
    SCOPED_TRACE(
        base::StringPrintf("index %" PRIuS ", size %" PRIuS, index, size));

    std::string line_0(size, '$');
    line_0.push_back('\n');

    StringFile string_file;
    string_file.SetString(line_0);
    FileLineReader file_line_reader(&string_file);

    std::string line;
    ASSERT_EQ(FileLineReader::Result::kSuccess,
              file_line_reader.GetLine(&line));
    EXPECT_EQ(line_0, line);
    EXPECT_EQ(FileLineReader::Result::kEndOfFile,
              file_line_reader.GetLine(&line));

    // The file is still at EOF.
    EXPECT_EQ(FileLineReader::Result::kEndOfFile,
              file_line_reader.GetLine(&line));

    std::string line_1(size, '@');
    line_1.push_back('\n');

    string_file.SetString(line_0 + line_1);
    ASSERT_EQ(FileLineReader::Result::kSuccess,
              file_line_reader.GetLine(&line));
    EXPECT_EQ(line_0, line);
    ASSERT_EQ(FileLineReader::Result::kSuccess,
              file_line_reader.GetLine(&line));
    EXPECT_EQ(line_1, line);
    EXPECT_EQ(FileLineReader::Result::kEndOfFile,
              file_line_reader.GetLine(&line));

    // The file is still at EOF.
    EXPECT_EQ(FileLineReader::Result::kEndOfFile,
              file_line_reader.GetLine(&line));

    line_1[size] = '?';

    string_file.SetString(line_0 + line_1);
    ASSERT_EQ(FileLineReader::Result::kSuccess,
              file_line_reader.GetLine(&line));
    EXPECT_EQ(line_0, line);
    ASSERT_EQ(FileLineReader::Result::kSuccess,
              file_line_reader.GetLine(&line));
    EXPECT_EQ(line_1, line);
    EXPECT_EQ(FileLineReader::Result::kEndOfFile,
              file_line_reader.GetLine(&line));

    // The file is still at EOF.
    EXPECT_EQ(FileLineReader::Result::kEndOfFile,
              file_line_reader.GetLine(&line));
  }
}

}  // namespace
}  // namespace test
}  // namespace crashpad
