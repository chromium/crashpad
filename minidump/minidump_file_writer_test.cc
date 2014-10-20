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

#include "minidump/minidump_file_writer.h"

#include <dbghelp.h>

#include <string>

#include "base/basictypes.h"
#include "gtest/gtest.h"
#include "minidump/minidump_stream_writer.h"
#include "minidump/minidump_writable.h"
#include "minidump/test/minidump_file_writer_test_util.h"
#include "util/file/file_writer.h"
#include "util/file/string_file_writer.h"

namespace crashpad {
namespace test {
namespace {

TEST(MinidumpFileWriter, Empty) {
  MinidumpFileWriter minidump_file;
  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file.WriteEverything(&file_writer));
  ASSERT_EQ(sizeof(MINIDUMP_HEADER), file_writer.string().size());

  const MINIDUMP_HEADER* header =
      reinterpret_cast<const MINIDUMP_HEADER*>(&file_writer.string()[0]);

  ASSERT_NO_FATAL_FAILURE(VerifyMinidumpHeader(header, 0, 0));
}

class TestStream final : public internal::MinidumpStreamWriter {
 public:
  TestStream(MinidumpStreamType stream_type,
             size_t stream_size,
             uint8_t stream_value)
      : stream_data_(stream_size, stream_value), stream_type_(stream_type) {}

  ~TestStream() {}

  // MinidumpStreamWriter:
  MinidumpStreamType StreamType() const override {
    return stream_type_;
  }

 protected:
  // MinidumpWritable:
  size_t SizeOfObject() override {
    EXPECT_GE(state(), kStateFrozen);
    return stream_data_.size();
  }

  bool WriteObject(FileWriterInterface* file_writer) override {
    EXPECT_EQ(state(), kStateWritable);
    return file_writer->Write(&stream_data_[0], stream_data_.size());
  }

 private:
  std::string stream_data_;
  MinidumpStreamType stream_type_;

  DISALLOW_COPY_AND_ASSIGN(TestStream);
};

TEST(MinidumpFileWriter, OneStream) {
  MinidumpFileWriter minidump_file;
  const time_t kTimestamp = 0x155d2fb8;
  minidump_file.SetTimestamp(kTimestamp);

  const size_t kStreamSize = 5;
  const MinidumpStreamType kStreamType = static_cast<MinidumpStreamType>(0x4d);
  const uint8_t kStreamValue = 0x5a;
  TestStream stream(kStreamType, kStreamSize, kStreamValue);
  minidump_file.AddStream(&stream);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file.WriteEverything(&file_writer));

  const size_t kDirectoryOffset = sizeof(MINIDUMP_HEADER);
  const size_t kStreamOffset = kDirectoryOffset + sizeof(MINIDUMP_DIRECTORY);
  const size_t kFileSize = kStreamOffset + kStreamSize;

  ASSERT_EQ(kFileSize, file_writer.string().size());

  const MINIDUMP_HEADER* header =
      reinterpret_cast<const MINIDUMP_HEADER*>(&file_writer.string()[0]);

  ASSERT_NO_FATAL_FAILURE(VerifyMinidumpHeader(header, 1, kTimestamp));

  const MINIDUMP_DIRECTORY* directory =
      reinterpret_cast<const MINIDUMP_DIRECTORY*>(
          &file_writer.string()[kDirectoryOffset]);

  EXPECT_EQ(kStreamType, directory->StreamType);
  EXPECT_EQ(kStreamSize, directory->Location.DataSize);
  EXPECT_EQ(kStreamOffset, directory->Location.Rva);

  const uint8_t* stream_data =
      reinterpret_cast<const uint8_t*>(&file_writer.string()[kStreamOffset]);

  std::string expected_stream(kStreamSize, kStreamValue);
  EXPECT_EQ(0, memcmp(stream_data, expected_stream.c_str(), kStreamSize));
}

TEST(MinidumpFileWriter, ThreeStreams) {
  MinidumpFileWriter minidump_file;
  const time_t kTimestamp = 0x155d2fb8;
  minidump_file.SetTimestamp(kTimestamp);

  const size_t kStream1Size = 5;
  const MinidumpStreamType kStream1Type = static_cast<MinidumpStreamType>(0x6d);
  const uint8_t kStream1Value = 0x5a;
  TestStream stream1(kStream1Type, kStream1Size, kStream1Value);
  minidump_file.AddStream(&stream1);

  // Make the second stream’s type be a smaller quantity than the first stream’s
  // to test that the streams show up in the order that they were added, not in
  // numeric order.
  const size_t kStream2Size = 3;
  const MinidumpStreamType kStream2Type = static_cast<MinidumpStreamType>(0x4d);
  const uint8_t kStream2Value = 0xa5;
  TestStream stream2(kStream2Type, kStream2Size, kStream2Value);
  minidump_file.AddStream(&stream2);

  const size_t kStream3Size = 1;
  const MinidumpStreamType kStream3Type = static_cast<MinidumpStreamType>(0x7e);
  const uint8_t kStream3Value = 0x36;
  TestStream stream3(kStream3Type, kStream3Size, kStream3Value);
  minidump_file.AddStream(&stream3);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file.WriteEverything(&file_writer));

  const size_t kDirectoryOffset = sizeof(MINIDUMP_HEADER);
  const size_t kStream1Offset =
      kDirectoryOffset + 3 * sizeof(MINIDUMP_DIRECTORY);
  const size_t kStream2Padding = 3;
  const size_t kStream2Offset = kStream1Offset + kStream1Size + kStream2Padding;
  const size_t kStream3Padding = 1;
  const size_t kStream3Offset = kStream2Offset + kStream2Size + kStream3Padding;
  const size_t kFileSize = kStream3Offset + kStream3Size;

  ASSERT_EQ(kFileSize, file_writer.string().size());

  const MINIDUMP_HEADER* header =
      reinterpret_cast<const MINIDUMP_HEADER*>(&file_writer.string()[0]);

  ASSERT_NO_FATAL_FAILURE(VerifyMinidumpHeader(header, 3, kTimestamp));

  const MINIDUMP_DIRECTORY* directory =
      reinterpret_cast<const MINIDUMP_DIRECTORY*>(
          &file_writer.string()[kDirectoryOffset]);

  EXPECT_EQ(kStream1Type, directory[0].StreamType);
  EXPECT_EQ(kStream1Size, directory[0].Location.DataSize);
  EXPECT_EQ(kStream1Offset, directory[0].Location.Rva);
  EXPECT_EQ(kStream2Type, directory[1].StreamType);
  EXPECT_EQ(kStream2Size, directory[1].Location.DataSize);
  EXPECT_EQ(kStream2Offset, directory[1].Location.Rva);
  EXPECT_EQ(kStream3Type, directory[2].StreamType);
  EXPECT_EQ(kStream3Size, directory[2].Location.DataSize);
  EXPECT_EQ(kStream3Offset, directory[2].Location.Rva);

  const uint8_t* stream1_data =
      reinterpret_cast<const uint8_t*>(&file_writer.string()[kStream1Offset]);

  std::string expected_stream1(kStream1Size, kStream1Value);
  EXPECT_EQ(0, memcmp(stream1_data, expected_stream1.c_str(), kStream1Size));

  const int kZeroes[16] = {};
  ASSERT_GE(sizeof(kZeroes), kStream2Padding);
  EXPECT_EQ(0, memcmp(stream1_data + kStream1Size, kZeroes, kStream2Padding));

  const uint8_t* stream2_data =
      reinterpret_cast<const uint8_t*>(&file_writer.string()[kStream2Offset]);

  std::string expected_stream2(kStream2Size, kStream2Value);
  EXPECT_EQ(0, memcmp(stream2_data, expected_stream2.c_str(), kStream2Size));

  ASSERT_GE(sizeof(kZeroes), kStream3Padding);
  EXPECT_EQ(0, memcmp(stream2_data + kStream2Size, kZeroes, kStream3Padding));

  const uint8_t* stream3_data =
      reinterpret_cast<const uint8_t*>(&file_writer.string()[kStream3Offset]);

  std::string expected_stream3(kStream3Size, kStream3Value);
  EXPECT_EQ(0, memcmp(stream3_data, expected_stream3.c_str(), kStream3Size));
}

TEST(MinidumpFileWriter, ZeroLengthStream) {
  MinidumpFileWriter minidump_file;

  const size_t kStreamSize = 0;
  const MinidumpStreamType kStreamType = static_cast<MinidumpStreamType>(0x4d);
  TestStream stream(kStreamType, kStreamSize, 0);
  minidump_file.AddStream(&stream);

  StringFileWriter file_writer;
  ASSERT_TRUE(minidump_file.WriteEverything(&file_writer));

  const size_t kDirectoryOffset = sizeof(MINIDUMP_HEADER);
  const size_t kStreamOffset = kDirectoryOffset + sizeof(MINIDUMP_DIRECTORY);
  const size_t kFileSize = kStreamOffset + kStreamSize;

  ASSERT_EQ(kFileSize, file_writer.string().size());

  const MINIDUMP_HEADER* header =
      reinterpret_cast<const MINIDUMP_HEADER*>(&file_writer.string()[0]);

  ASSERT_NO_FATAL_FAILURE(VerifyMinidumpHeader(header, 1, 0));

  const MINIDUMP_DIRECTORY* directory =
      reinterpret_cast<const MINIDUMP_DIRECTORY*>(
          &file_writer.string()[kDirectoryOffset]);

  EXPECT_EQ(kStreamType, directory->StreamType);
  EXPECT_EQ(kStreamSize, directory->Location.DataSize);
  EXPECT_EQ(kStreamOffset, directory->Location.Rva);
}

TEST(MinidumpFileWriterDeathTest, SameStreamType) {
  MinidumpFileWriter minidump_file;

  const size_t kStream1Size = 5;
  const MinidumpStreamType kStream1Type = static_cast<MinidumpStreamType>(0x4d);
  const uint8_t kStream1Value = 0x5a;
  TestStream stream1(kStream1Type, kStream1Size, kStream1Value);
  minidump_file.AddStream(&stream1);

  // It is an error to add a second stream of the same type.
  const size_t kStream2Size = 3;
  const MinidumpStreamType kStream2Type = static_cast<MinidumpStreamType>(0x4d);
  const uint8_t kStream2Value = 0xa5;
  TestStream stream2(kStream2Type, kStream2Size, kStream2Value);
  ASSERT_DEATH(minidump_file.AddStream(&stream2), "already present");
}

}  // namespace
}  // namespace test
}  // namespace crashpad
