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

#include "util/stream/log_file_processor.h"

#include <algorithm>
#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "gtest/gtest.h"
#include "test/scoped_temp_dir.h"
#include "util/file/file_reader.h"
#include "util/stream/file_output_stream.h"

namespace crashpad {
namespace test {
namespace {

constexpr size_t kBufferSize = 4096;

class LogFileProcessorTest : public testing::Test {
 public:
  LogFileProcessorTest() {}

  void Verify(size_t size) {
    FileReader decoded;
    EXPECT_TRUE(decoded.Open(decoded_));
    const uint8_t* cur = deterministic_input_.get();
    FileOperationResult read_result;
    do {
      uint8_t buffer[kBufferSize];
      read_result = decoded.Read(buffer, kBufferSize);
      EXPECT_GE(read_result, 0);

      if (read_result > 0)
        EXPECT_EQ(memcmp(cur, buffer, read_result), 0);
      cur += read_result;
    } while (read_result > 0);
    EXPECT_EQ(cur - size, deterministic_input_.get());
  }

  const uint8_t* BuildDeterministicInput(size_t size) {
    deterministic_input_ = std::make_unique<uint8_t[]>(size);
    uint8_t* deterministic_input_base = deterministic_input_.get();
    while (size-- > 0)
      deterministic_input_base[size] = static_cast<uint8_t>(size);
    return deterministic_input_base;
  }

  void GenerateOrgFile(size_t size) {
    FileOutputStream out(org_);
    const uint8_t* buf = BuildDeterministicInput(size);
    while (size > 0) {
      size_t m = std::min(kBufferSize, size);
      EXPECT_TRUE(out.Write(buf, m));
      size -= m;
      buf += m;
    }
    EXPECT_TRUE(out.Flush());
  }

  LogFileProcessor* encode_processor() const { return encode_processor_.get(); }

  LogFileProcessor* decode_processor() const { return decode_processor_.get(); }

 protected:
  void SetUp() override {
    temp_dir_ = std::make_unique<ScopedTempDir>();
    org_ = base::FilePath(temp_dir_->path().Append(FILE_PATH_LITERAL("org")));
    encoded_ =
        base::FilePath(temp_dir_->path().Append(FILE_PATH_LITERAL("encoded")));
    decoded_ =
        base::FilePath(temp_dir_->path().Append(FILE_PATH_LITERAL("decoded")));
    encode_processor_ = std::make_unique<LogFileProcessor>(
        LogFileProcessor::Mode::kEncode, org_, encoded_);
    decode_processor_ = std::make_unique<LogFileProcessor>(
        LogFileProcessor::Mode::kDecode, encoded_, decoded_);
  }

 private:
  std::unique_ptr<ScopedTempDir> temp_dir_;
  base::FilePath org_;
  base::FilePath encoded_;
  base::FilePath decoded_;
  std::unique_ptr<LogFileProcessor> encode_processor_;
  std::unique_ptr<LogFileProcessor> decode_processor_;
  std::unique_ptr<uint8_t[]> deterministic_input_;
};

TEST_F(LogFileProcessorTest, ProcessShortFile) {
  GenerateOrgFile(kBufferSize - 512);
  EXPECT_TRUE(encode_processor()->Process());
  EXPECT_TRUE(decode_processor()->Process());
  Verify(kBufferSize - 512);
}

TEST_F(LogFileProcessorTest, ProcessLongFile) {
  GenerateOrgFile(kBufferSize + 512);
  EXPECT_TRUE(encode_processor()->Process());
  EXPECT_TRUE(decode_processor()->Process());
  Verify(kBufferSize + 512);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
