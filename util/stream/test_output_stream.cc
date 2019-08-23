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

#include "util/stream/test_output_stream.h"

namespace crashpad {

TestOutputStream::TestOutputStream()
    : last_written_data_(),
      all_data_(),
      write_called_(false),
      flush_called_(false) {}

TestOutputStream::~TestOutputStream() = default;

bool TestOutputStream::Write(const uint8_t* data, size_t size) {
  last_written_data_ = std::vector<uint8_t>(size);
  memcpy(last_written_data_.data(), data, size);
  write_called_ = true;

  // Save the data to |all_data_|.
  size_t old_size = all_data_.size();
  all_data_.resize(size + old_size);
  memcpy(all_data_.data() + old_size, data, size);
  return true;
}

bool TestOutputStream::Flush() {
  flush_called_ = true;
  return true;
}

void TestOutputStream::Reset() {
  last_written_data_.clear();
  write_called_ = false;
  flush_called_ = false;
}

}  // namespace crashpad
