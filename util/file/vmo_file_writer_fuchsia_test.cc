// Copyright 2019 The Crashpad Authors. All rights reserved.
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

#include "util/file/vmo_file_writer_fuchsia.h"

#include "gtest/gtest.h"
#include "test/errors.h"

namespace crashpad {
namespace test {
namespace {

const char kSomeData[] =      "abcdefghi";
const char kSomeOtherData[] = "123456";
constexpr int kSomeDataLength = sizeof(kSomeData) / sizeof(char);

TEST(VMOFileWriter, SimpleWrite) {
  char buf[1024] = {};

  VMOFileWriter writer;
  ASSERT_TRUE(writer.Write(kSomeData, sizeof(kSomeData)));

  zx::vmo vmo;
  ASSERT_EQ(writer.GenerateVMO(&vmo), ZX_OK);
  ASSERT_TRUE(vmo.is_valid());

  ASSERT_EQ(vmo.read(buf, 0, sizeof(kSomeData)), ZX_OK);
  ASSERT_EQ(std::string(buf), kSomeData);

  // Replace some data.
  ASSERT_EQ(writer.Seek(0, SEEK_CUR), kSomeDataLength);
  writer.Seek(0, SEEK_SET);
  ASSERT_TRUE(writer.Write(kSomeOtherData, sizeof(kSomeOtherData)));

  ASSERT_EQ(writer.GenerateVMO(&vmo), ZX_OK);
  ASSERT_TRUE(vmo.is_valid());

  ASSERT_EQ(vmo.read(buf, 0, sizeof(kSomeOtherData)), ZX_OK);
  ASSERT_EQ(std::string(buf), kSomeOtherData);

  // The buffer should grow.
  writer.Seek(-1, SEEK_CUR);    // We eat up the '\0'.
  ASSERT_TRUE(writer.Write(kSomeData, sizeof(kSomeData)));
  ASSERT_EQ((size_t)writer.Seek(0, SEEK_END), sizeof(kSomeData) + sizeof(kSomeOtherData) - 1);

  ASSERT_EQ(writer.GenerateVMO(&vmo), ZX_OK);
  ASSERT_TRUE(vmo.is_valid());

  constexpr char kExpectedResult[] = "123456abcdefghi";
  ASSERT_EQ(vmo.read(buf, 0, sizeof(kExpectedResult)), ZX_OK);
  ASSERT_EQ(std::string(buf), kExpectedResult);
}

TEST(VMOFileWriter, IOVecs) {
  std::vector<WritableIoVec> io_vecs;
  for (int i = 0; i < 4; i++) {
    // Sneakely don't add the \0.
    io_vecs.push_back({kSomeOtherData, sizeof(kSomeOtherData) - 1});
  }

  VMOFileWriter writer;
  ASSERT_TRUE(writer.WriteIoVec(&io_vecs));

  zx::vmo vmo;
  ASSERT_EQ(writer.GenerateVMO(&vmo), ZX_OK);
  ASSERT_TRUE(vmo.is_valid());

  size_t seek = writer.Seek(0, SEEK_CUR);
  ASSERT_EQ(seek, 4 * sizeof(kSomeOtherData) - 4);

  char buf[1024] = {};
  ASSERT_EQ(vmo.read(buf, 0, seek), ZX_OK);
  ASSERT_EQ(strcmp(buf, "123456123456123456123456"), 0) << buf;
}

}  // namespace
}  // namespace test
}  // namespace crashpad
