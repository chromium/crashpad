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

#include "util/stream/z_stream.h"

#include <cstdlib>

#include "base/logging.h"
#include "gtest/gtest.h"
#include "util/stream/output_stream_test_helper.h"

namespace crashpad {

class ZStreamTest : public testing::Test {
 public:
  ZStreamTest()
      : z_stream_(std::make_unique<ZStream>(
                      std::make_unique<OutputStreamTestHelper>(),
                      ZStream::Mode::kInflate),
                  ZStream::Mode::kDeflate) {}

 protected:
  void SetUp() override { GetOutputStreamTestHelper()->Reset(); }

  const uint8_t* BuildInput(size_t size) {
    input_ = std::make_unique<uint8_t[]>(size);
    srand(time(nullptr));
    while (size-- > 0)
      input_.get()[size] = rand() % 256;
    return input_.get();
  }

  OutputStreamTestHelper* GetOutputStreamTestHelper() const {
    return static_cast<OutputStreamTestHelper*>(
        z_stream_.GetOutputStreamForTesting()->GetOutputStreamForTesting());
  }

  ZStream* z_stream() { return &z_stream_; }

 private:
  ZStream z_stream_;
  std::unique_ptr<uint8_t[]> input_;
};

TEST_F(ZStreamTest, WriteShortData) {
  size_t len = 10;
  const uint8_t* input = BuildInput(len);
  EXPECT_TRUE(z_stream()->Write(input, len));
  EXPECT_TRUE(z_stream()->Flush());
  EXPECT_EQ(len, GetOutputStreamTestHelper()->size());
  EXPECT_EQ(0, memcmp(GetOutputStreamTestHelper()->GetData(), input, len));
}

TEST_F(ZStreamTest, WriteLongDataOneTime) {
  // One character more than the size of buffer.
  size_t len = ZStream::kBufferLen;
  const uint8_t* input = BuildInput(len);
  EXPECT_TRUE(z_stream()->Write(input, len));
  EXPECT_TRUE(z_stream()->Flush());
  EXPECT_EQ(0, memcmp(GetOutputStreamTestHelper()->GetAllData(), input, len));
}

TEST_F(ZStreamTest, WriteLongDataMultipleTimes) {
  size_t len = ZStream::kBufferLen * 10;
  const uint8_t* input = BuildInput(len);

  // Call write random times.
  while (len) {
    size_t cur = rand() % ZStream::kBufferLen;
    cur = cur < len ? cur : len;
    EXPECT_TRUE(z_stream()->Write(input, cur));
    len -= cur;
  }
  EXPECT_TRUE(z_stream()->Flush());
  EXPECT_EQ(0, memcmp(GetOutputStreamTestHelper()->GetAllData(), input, len));
}

}  // namespace crashpad
