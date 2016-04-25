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

#include "minidump/minidump_user_stream_writer.h"

#include <string>
#include <utility>

#include "base/memory/ptr_util.h"
#include "gtest/gtest.h"
#include "minidump/minidump_file_writer.h"
#include "minidump/test/minidump_file_writer_test_util.h"
#include "minidump/test/minidump_writable_test_util.h"
#include "snapshot/test/test_memory_snapshot.h"
#include "util/file/string_file.h"

namespace crashpad {
namespace test {
namespace {

// The user stream is expected to be the only stream.
void GetUserStream(const std::string& file_contents,
                   MINIDUMP_LOCATION_DESCRIPTOR* user_stream_location,
                   uint32_t stream_type) {
  const size_t kDirectoryOffset = sizeof(MINIDUMP_HEADER);
  const size_t kUserStreamOffset =
      kDirectoryOffset + sizeof(MINIDUMP_DIRECTORY);

  const MINIDUMP_DIRECTORY* directory;
  const MINIDUMP_HEADER* header =
      MinidumpHeaderAtStart(file_contents, &directory);
  ASSERT_NO_FATAL_FAILURE(VerifyMinidumpHeader(header, 1, 0));
  ASSERT_TRUE(directory);

  const size_t kDirectoryIndex = 0;

  ASSERT_EQ(stream_type, directory[kDirectoryIndex].StreamType);
  EXPECT_EQ(kUserStreamOffset, directory[kDirectoryIndex].Location.Rva);
  *user_stream_location = directory[kDirectoryIndex].Location;
}

TEST(MinidumpUserStreamWriter, NoData) {
  MinidumpFileWriter minidump_file_writer;
  auto user_stream_writer = base::WrapUnique(new MinidumpUserStreamWriter());
  const uint32_t kTestStreamId = 0x123456;
  auto stream =
      base::WrapUnique(new UserMinidumpStream(kTestStreamId, nullptr));
  user_stream_writer->InitializeFromSnapshot(stream.get());
  minidump_file_writer.AddStream(std::move(user_stream_writer));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  ASSERT_EQ(sizeof(MINIDUMP_HEADER) + sizeof(MINIDUMP_DIRECTORY),
            string_file.string().size());

  MINIDUMP_LOCATION_DESCRIPTOR user_stream_location;
  ASSERT_NO_FATAL_FAILURE(GetUserStream(
      string_file.string(), &user_stream_location, kTestStreamId));
  EXPECT_EQ(0u, user_stream_location.DataSize);
}

TEST(MinidumpUserStreamWriter, OneStream) {
  MinidumpFileWriter minidump_file_writer;
  auto user_stream_writer = base::WrapUnique(new MinidumpUserStreamWriter());
  const uint32_t kTestStreamId = 0x123456;

  TestMemorySnapshot* test_data = new TestMemorySnapshot();
  test_data->SetAddress(97865);
  const size_t kStreamSize = 128;
  test_data->SetSize(kStreamSize);
  test_data->SetValue('c');
  auto stream =
      base::WrapUnique(new UserMinidumpStream(kTestStreamId, test_data));
  user_stream_writer->InitializeFromSnapshot(stream.get());
  minidump_file_writer.AddStream(std::move(user_stream_writer));

  StringFile string_file;
  ASSERT_TRUE(minidump_file_writer.WriteEverything(&string_file));

  ASSERT_EQ(sizeof(MINIDUMP_HEADER) + sizeof(MINIDUMP_DIRECTORY) + kStreamSize,
            string_file.string().size());

  MINIDUMP_LOCATION_DESCRIPTOR user_stream_location;
  ASSERT_NO_FATAL_FAILURE(GetUserStream(
      string_file.string(), &user_stream_location, kTestStreamId));
  EXPECT_EQ(kStreamSize, user_stream_location.DataSize);
  const std::string stream_data = string_file.string().substr(
      user_stream_location.Rva, user_stream_location.DataSize);
  EXPECT_EQ(std::string(kStreamSize, 'c'), stream_data);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
