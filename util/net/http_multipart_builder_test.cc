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

#include "util/net/http_multipart_builder.h"

#include <vector>

#include "gtest/gtest.h"
#include "util/net/http_body.h"
#include "util/net/http_body_test_util.h"

namespace crashpad {
namespace test {
namespace {

std::vector<std::string> SplitCRLF(const std::string& string) {
  std::vector<std::string> lines;
  size_t last_line = 0;
  for (size_t i = 0; i < string.length(); ++i) {
    if (string[i] == '\r' && i+1 < string.length() && string[i+1] == '\n') {
      lines.push_back(string.substr(last_line, i - last_line));
      last_line = i + 2;
      ++i;
    }
  }
  // Append any remainder.
  if (last_line < string.length()) {
    lines.push_back(string.substr(last_line));
  }
  return lines;
}

// In the tests below, the form data pairs don’t appear in the order they were
// added. The current implementation uses a std::map which sorts keys, so the
// entires appear in alphabetical order. However, this is an implementation
// detail, and it’s OK if the writer stops sorting in this order. Testing for
// a specific order is just the easiest way to write this test while the writer
// will output things in a known order.

TEST(HTTPMultipartBuilder, ThreeStringFields) {
  HTTPMultipartBuilder builder;

  const char kKey1[] = "key1";
  const char kValue1[] = "test";
  builder.SetFormData(kKey1, kValue1);

  const char kKey2[] = "key2";
  const char kValue2[] = "This is another test.";
  builder.SetFormData(kKey2, kValue2);

  const char kKey3[] = "key-three";
  const char kValue3[] = "More tests";
  builder.SetFormData(kKey3, kValue3);

  scoped_ptr<HTTPBodyStream> body(builder.GetBodyStream());
  ASSERT_TRUE(body.get());
  std::string contents = ReadStreamToString(body.get());
  auto lines = SplitCRLF(contents);
  auto lines_it = lines.begin();

  // The first line is the boundary. All subsequent boundaries must match this.
  const std::string& boundary = *lines_it++;
  EXPECT_GE(boundary.length(), 1u);
  EXPECT_LE(boundary.length(), 70u);

  EXPECT_EQ("Content-Disposition: form-data; name=\"key-three\"", *lines_it++);
  EXPECT_EQ("", *lines_it++);
  EXPECT_EQ(kValue3, *lines_it++);

  EXPECT_EQ(boundary, *lines_it++);
  EXPECT_EQ("Content-Disposition: form-data; name=\"key1\"", *lines_it++);
  EXPECT_EQ("", *lines_it++);
  EXPECT_EQ(kValue1, *lines_it++);

  EXPECT_EQ(boundary, *lines_it++);
  EXPECT_EQ("Content-Disposition: form-data; name=\"key2\"", *lines_it++);
  EXPECT_EQ("", *lines_it++);
  EXPECT_EQ(kValue2, *lines_it++);

  EXPECT_EQ(boundary + "--", *lines_it++);

  EXPECT_EQ(lines.end(), lines_it);
}

TEST(HTTPMultipartBuilder, ThreeFileAttachments) {
  HTTPMultipartBuilder builder;
  // TODO(rsesek): Use a more robust mechanism to locate testdata
  // <https://code.google.com/p/crashpad/issues/detail?id=4>.
  builder.SetFileAttachment("first", "minidump.dmp",
      base::FilePath("util/net/testdata/ascii_http_body.txt"), "");
  builder.SetFileAttachment("second", "minidump.dmp",
      base::FilePath("util/net/testdata/ascii_http_body.txt"), "text/plain");
  builder.SetFileAttachment("\"third 50% silly\"", "test%foo.txt",
      base::FilePath("util/net/testdata/ascii_http_body.txt"), "text/plain");

  const char kFileContents[] = "This is a test.\n";

  scoped_ptr<HTTPBodyStream> body(builder.GetBodyStream());
  ASSERT_TRUE(body.get());
  std::string contents = ReadStreamToString(body.get());
  auto lines = SplitCRLF(contents);
  auto lines_it = lines.begin();

  const std::string& boundary = *lines_it++;
  EXPECT_GE(boundary.length(), 1u);
  EXPECT_LE(boundary.length(), 70u);

  EXPECT_EQ("Content-Disposition: form-data; "
                "name=\"%22third 50%25 silly%22\"; filename=\"test%25foo.txt\"",
            *lines_it++);
  EXPECT_EQ("Content-Type: text/plain", *lines_it++);
  EXPECT_EQ("", *lines_it++);
  EXPECT_EQ(kFileContents, *lines_it++);

  EXPECT_EQ(boundary, *lines_it++);
  EXPECT_EQ("Content-Disposition: form-data; "
                "name=\"first\"; filename=\"minidump.dmp\"",
            *lines_it++);
  EXPECT_EQ("Content-Type: application/octet-stream", *lines_it++);
  EXPECT_EQ("", *lines_it++);
  EXPECT_EQ(kFileContents, *lines_it++);

  EXPECT_EQ(boundary, *lines_it++);
  EXPECT_EQ("Content-Disposition: form-data; "
                "name=\"second\"; filename=\"minidump.dmp\"",
            *lines_it++);
  EXPECT_EQ("Content-Type: text/plain", *lines_it++);
  EXPECT_EQ("", *lines_it++);
  EXPECT_EQ(kFileContents, *lines_it++);

  EXPECT_EQ(boundary + "--", *lines_it++);

  EXPECT_EQ(lines.end(), lines_it);
}

TEST(HTTPMultipartBuilder, OverwriteFormDataWithEscapedKey) {
  HTTPMultipartBuilder builder;
  const char kKey[] = "a 100% \"silly\"\r\ntest";
  builder.SetFormData(kKey, "some dummy value");
  builder.SetFormData(kKey, "overwrite");
  scoped_ptr<HTTPBodyStream> body(builder.GetBodyStream());
  ASSERT_TRUE(body.get());
  std::string contents = ReadStreamToString(body.get());
  auto lines = SplitCRLF(contents);
  auto lines_it = lines.begin();

  const std::string& boundary = *lines_it++;
  EXPECT_GE(boundary.length(), 1u);
  EXPECT_LE(boundary.length(), 70u);

  EXPECT_EQ(
      "Content-Disposition: form-data; name=\"a 100%25 %22silly%22%0d%0atest\"",
      *lines_it++);
  EXPECT_EQ("", *lines_it++);
  EXPECT_EQ("overwrite", *lines_it++);
  EXPECT_EQ(boundary + "--", *lines_it++);
  EXPECT_EQ(lines.end(), lines_it);
}

TEST(HTTPMultipartBuilder, OverwriteFileAttachment) {
  HTTPMultipartBuilder builder;
  const char kValue[] = "1 2 3 test";
  builder.SetFormData("a key", kValue);
  // TODO(rsesek): Use a more robust mechanism to locate testdata
  // <https://code.google.com/p/crashpad/issues/detail?id=4>.
  builder.SetFileAttachment("minidump", "minidump.dmp",
      base::FilePath("util/net/testdata/binary_http_body.dat"), "");
  builder.SetFileAttachment("minidump2", "minidump.dmp",
      base::FilePath("util/net/testdata/binary_http_body.dat"), "");
  builder.SetFileAttachment("minidump", "minidump.dmp",
      base::FilePath("util/net/testdata/ascii_http_body.txt"), "text/plain");
  scoped_ptr<HTTPBodyStream> body(builder.GetBodyStream());
  ASSERT_TRUE(body.get());
  std::string contents = ReadStreamToString(body.get());
  auto lines = SplitCRLF(contents);
  auto lines_it = lines.begin();

  const std::string& boundary = *lines_it++;
  EXPECT_GE(boundary.length(), 1u);
  EXPECT_LE(boundary.length(), 70u);

  EXPECT_EQ("Content-Disposition: form-data; name=\"a key\"", *lines_it++);
  EXPECT_EQ("", *lines_it++);
  EXPECT_EQ(kValue, *lines_it++);

  EXPECT_EQ(boundary, *lines_it++);
  EXPECT_EQ("Content-Disposition: form-data; "
                "name=\"minidump\"; filename=\"minidump.dmp\"",
            *lines_it++);
  EXPECT_EQ("Content-Type: text/plain", *lines_it++);
  EXPECT_EQ("", *lines_it++);
  EXPECT_EQ("This is a test.\n", *lines_it++);

  EXPECT_EQ(boundary, *lines_it++);
  EXPECT_EQ("Content-Disposition: form-data; "
                "name=\"minidump2\"; filename=\"minidump.dmp\"",
            *lines_it++);
  EXPECT_EQ("Content-Type: application/octet-stream", *lines_it++);
  EXPECT_EQ("", *lines_it++);
  EXPECT_EQ("\xFE\xED\xFA\xCE\xA1\x1A\x15", *lines_it++);

  EXPECT_EQ(boundary + "--", *lines_it++);

  EXPECT_EQ(lines.end(), lines_it);
}

TEST(HTTPMultipartBuilder, SharedFormDataAndAttachmentKeyNamespace) {
  HTTPMultipartBuilder builder;
  const char kValue1[] = "11111";
  builder.SetFormData("one", kValue1);
  builder.SetFileAttachment("minidump", "minidump.dmp",
      base::FilePath("util/net/testdata/ascii_http_body.txt"), "");
  const char kValue2[] = "this is not a file";
  builder.SetFormData("minidump", kValue2);

  scoped_ptr<HTTPBodyStream> body(builder.GetBodyStream());
  ASSERT_TRUE(body.get());
  std::string contents = ReadStreamToString(body.get());
  auto lines = SplitCRLF(contents);
  auto lines_it = lines.begin();

  const std::string& boundary = *lines_it++;
  EXPECT_GE(boundary.length(), 1u);
  EXPECT_LE(boundary.length(), 70u);

  EXPECT_EQ("Content-Disposition: form-data; name=\"minidump\"", *lines_it++);
  EXPECT_EQ("", *lines_it++);
  EXPECT_EQ(kValue2, *lines_it++);

  EXPECT_EQ(boundary, *lines_it++);
  EXPECT_EQ("Content-Disposition: form-data; name=\"one\"", *lines_it++);
  EXPECT_EQ("", *lines_it++);
  EXPECT_EQ(kValue1, *lines_it++);

  EXPECT_EQ(boundary + "--", *lines_it++);

  EXPECT_EQ(lines.end(), lines_it);
}

TEST(HTTPMultipartBuilderDeathTest, AssertUnsafeMIMEType) {
  HTTPMultipartBuilder builder;
  // Invalid and potentially dangerous:
  ASSERT_DEATH(builder.SetFileAttachment("", "", base::FilePath(), "\r\n"), "");
  ASSERT_DEATH(builder.SetFileAttachment("", "", base::FilePath(), "\""), "");
  ASSERT_DEATH(builder.SetFileAttachment("", "", base::FilePath(), "\x12"), "");
  ASSERT_DEATH(builder.SetFileAttachment("", "", base::FilePath(), "<>"), "");
  // Invalid but safe:
  builder.SetFileAttachment("", "", base::FilePath(), "0/totally/-invalid.pdf");
  // Valid and safe:
  builder.SetFileAttachment("", "", base::FilePath(), "application/xml+xhtml");
}

}  // namespace
}  // namespace test
}  // namespace crashpad
