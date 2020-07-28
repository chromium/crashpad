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

#include "util/stream/zlib_output_stream.h"

#include <string.h>

#include <algorithm>

#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "gtest/gtest.h"
#include "util/stream/test_output_stream.h"

namespace crashpad {
namespace test {
namespace {

constexpr size_t kShortDataLength = 10;
constexpr size_t kLongDataLength = 4096 * 10;

class ZlibOutputStreamTest : public testing::Test {
 public:
  ZlibOutputStreamTest() : input_(), deterministic_input_() {
    auto test_output_stream = std::make_unique<TestOutputStream>();
    test_output_stream_ = test_output_stream.get();
    zlib_output_stream_ = std::make_unique<ZlibOutputStream>(
        ZlibOutputStream::Mode::kCompress,
        std::make_unique<ZlibOutputStream>(ZlibOutputStream::Mode::kDecompress,
                                           std::move(test_output_stream)));
  }

  const uint8_t* BuildDeterministicInput(size_t size) {
    deterministic_input_ = std::make_unique<uint8_t[]>(size);
    uint8_t* deterministic_input_base = deterministic_input_.get();
    while (size-- > 0)
      deterministic_input_base[size] = static_cast<uint8_t>(size);
    return deterministic_input_base;
  }

  const uint8_t* BuildRandomInput(size_t size) {
    input_ = std::make_unique<uint8_t[]>(size);
    base::RandBytes(&input_[0], size);
    return input_.get();
  }

  const TestOutputStream& test_output_stream() const {
    return *test_output_stream_;
  }

  ZlibOutputStream* zlib_output_stream() const {
    return zlib_output_stream_.get();
  }

 private:
  std::unique_ptr<ZlibOutputStream> zlib_output_stream_;
  std::unique_ptr<uint8_t[]> input_;
  std::unique_ptr<uint8_t[]> deterministic_input_;
  TestOutputStream* test_output_stream_;  // weak, owned by zlib_output_stream_

  DISALLOW_COPY_AND_ASSIGN(ZlibOutputStreamTest);
};

TEST_F(ZlibOutputStreamTest, WriteDeterministicShortData) {
  const uint8_t* input = BuildDeterministicInput(kShortDataLength);
  EXPECT_TRUE(zlib_output_stream()->Write(input, kShortDataLength));
  EXPECT_TRUE(zlib_output_stream()->Flush());
  EXPECT_EQ(test_output_stream().last_written_data().size(), kShortDataLength);
  EXPECT_EQ(memcmp(test_output_stream().last_written_data().data(),
                   input,
                   kShortDataLength),
            0);
}

TEST_F(ZlibOutputStreamTest, WriteDeterministicLongDataOneTime) {
  const uint8_t* input = BuildDeterministicInput(kLongDataLength);
  EXPECT_TRUE(zlib_output_stream()->Write(input, kLongDataLength));
  EXPECT_TRUE(zlib_output_stream()->Flush());
  EXPECT_EQ(test_output_stream().all_data().size(), kLongDataLength);
  EXPECT_EQ(
      memcmp(test_output_stream().all_data().data(), input, kLongDataLength),
      0);
}

TEST_F(ZlibOutputStreamTest, WriteDeterministicLongDataMultipleTimes) {
  const uint8_t* input = BuildDeterministicInput(kLongDataLength);

  static constexpr size_t kWriteLengths[] = {
      4, 96, 40, kLongDataLength - 4 - 96 - 40};

  size_t offset = 0;
  for (size_t index = 0; index < base::size(kWriteLengths); ++index) {
    const size_t write_length = kWriteLengths[index];
    SCOPED_TRACE(base::StringPrintf(
        "offset %zu, write_length %zu", offset, write_length));
    EXPECT_TRUE(zlib_output_stream()->Write(input + offset, write_length));
    offset += write_length;
  }
  EXPECT_TRUE(zlib_output_stream()->Flush());
  EXPECT_EQ(test_output_stream().all_data().size(), kLongDataLength);
  EXPECT_EQ(
      memcmp(test_output_stream().all_data().data(), input, kLongDataLength),
      0);
}

TEST_F(ZlibOutputStreamTest, WriteShortData) {
  const uint8_t* input = BuildRandomInput(kShortDataLength);
  EXPECT_TRUE(zlib_output_stream()->Write(input, kShortDataLength));
  EXPECT_TRUE(zlib_output_stream()->Flush());
  EXPECT_EQ(memcmp(test_output_stream().last_written_data().data(),
                   input,
                   kShortDataLength),
            0);
  EXPECT_EQ(test_output_stream().last_written_data().size(), kShortDataLength);
}

TEST_F(ZlibOutputStreamTest, WriteLongDataOneTime) {
  const uint8_t* input = BuildRandomInput(kLongDataLength);
  EXPECT_TRUE(zlib_output_stream()->Write(input, kLongDataLength));
  EXPECT_TRUE(zlib_output_stream()->Flush());
  EXPECT_EQ(test_output_stream().all_data().size(), kLongDataLength);
  EXPECT_EQ(
      memcmp(test_output_stream().all_data().data(), input, kLongDataLength),
      0);
}

TEST_F(ZlibOutputStreamTest, WriteLongDataMultipleTimes) {
  const uint8_t* input = BuildRandomInput(kLongDataLength);

  // Call Write() a random number of times.
  size_t index = 0;
  while (index < kLongDataLength) {
    size_t write_length =
        std::min(static_cast<size_t>(base::RandInt(0, 4096 * 2)),
                 kLongDataLength - index);
    SCOPED_TRACE(
        base::StringPrintf("index %zu, write_length %zu", index, write_length));
    EXPECT_TRUE(zlib_output_stream()->Write(input + index, write_length));
    index += write_length;
  }
  EXPECT_TRUE(zlib_output_stream()->Flush());
  EXPECT_EQ(test_output_stream().all_data().size(), kLongDataLength);
  EXPECT_EQ(
      memcmp(test_output_stream().all_data().data(), input, kLongDataLength),
      0);
}

TEST_F(ZlibOutputStreamTest, NoWriteOrFlush) {
  EXPECT_EQ(test_output_stream().write_count(), 0u);
  EXPECT_EQ(test_output_stream().flush_count(), 0u);
  EXPECT_TRUE(test_output_stream().all_data().empty());
}

TEST_F(ZlibOutputStreamTest, FlushWithoutWrite) {
  EXPECT_TRUE(zlib_output_stream()->Flush());
  EXPECT_EQ(test_output_stream().write_count(), 0u);
  EXPECT_EQ(test_output_stream().flush_count(), 1u);
  EXPECT_TRUE(test_output_stream().all_data().empty());
}

TEST_F(ZlibOutputStreamTest, WriteEmptyData) {
  std::vector<uint8_t> empty_data;
  EXPECT_TRUE(zlib_output_stream()->Write(
      static_cast<const uint8_t*>(empty_data.data()), empty_data.size()));
  EXPECT_TRUE(zlib_output_stream()->Flush());
  EXPECT_TRUE(zlib_output_stream()->Flush());
  EXPECT_EQ(test_output_stream().write_count(), 0u);
  EXPECT_EQ(test_output_stream().flush_count(), 2u);
  EXPECT_TRUE(test_output_stream().all_data().empty());
}

TEST_F(ZlibOutputStreamTest, CorruptedDataNotHangFlush) {
  auto test_output_stream = std::make_unique<TestOutputStream>();
  auto* result = test_output_stream.get();
  ZlibOutputStream zlib_decompress_stream{ZlibOutputStream::Mode::kDecompress,
                                          std::move(test_output_stream)};

  // This corrupted data would result the ZlibOutputStream to wait for more data
  // when stream ends, therefore hang forever if not handle it correctly.
  static constexpr uint8_t kCorruptedData[] = {
      0x78, 0xda, 0xed, 0xbd, 0x0d, 0x5c, 0x4d, 0xc9, 0x1f, 0x3f, 0x7e, 0x4a,
      0x56, 0x48, 0xa5, 0x0d, 0x2d, 0xe1, 0x22, 0xc4, 0x56, 0x8a, 0x10, 0x42,
      0x12, 0x42, 0xc8, 0x73, 0xd6, 0x43, 0xa5, 0x6e, 0x4a, 0x4f, 0xd7, 0xed,
      0x01, 0xeb, 0x29, 0xcf, 0xd9, 0xb5, 0x2b, 0x96, 0x95, 0x15, 0x42, 0x9e,
      0xb3, 0xb2, 0x9e, 0xb2, 0x2c, 0x59, 0x59, 0x97, 0x4d, 0xe5, 0x31, 0x0f,
      0xa9, 0x84, 0x95, 0x5b, 0x88, 0xf5, 0x90, 0x5d, 0xb6, 0xdf, 0x67, 0xe6,
      0x9c, 0x73, 0xcf, 0x9c, 0x73, 0xef, 0x3d, 0xd2, 0xe1, 0xbb, 0xfb, 0xff,
      0xff, 0x7e, 0xb7, 0xd7, 0x34, 0xe7, 0xcc, 0x7c, 0xe6, 0xf3, 0x9e, 0xf9,
      0xcc, 0x67, 0x3e, 0xf3, 0x99, 0x99, 0x73, 0xee, 0x1d, 0xe2, 0x31, 0xc4};

  EXPECT_TRUE(
      zlib_decompress_stream.Write(kCorruptedData, sizeof(kCorruptedData)));
  EXPECT_NE(result->write_count(), 0u);
  // Verifies Flush failed, but doesn't hang on.
  EXPECT_FALSE(zlib_decompress_stream.Flush());
  // Calls Flush() to avoid DCHECK failure.
  result->Flush();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
