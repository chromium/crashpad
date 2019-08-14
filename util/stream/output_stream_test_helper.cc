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

#include "util/stream/output_stream_test_helper.h"

namespace crashpad {

OutputStreamTestHelper::OutputStreamTestHelper()
    : OutputStreamInterface(nullptr) {}

bool OutputStreamTestHelper::Write(const void* data, size_t size) {
  data_ = std::make_unique<uint8_t[]>(size);
  memcpy(data_.get(), data, size);
  size_ = size;
  write_called_ = true;

  // Save the data to |all_data_|.
  size_t new_size = size + total_size_;
  std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(new_size);
  if (all_data_)
    memcpy(buffer.get(), all_data_.get(), total_size_);

  memcpy(buffer.get() + total_size_, data, size);
  all_data_.swap(buffer);
  total_size_ = new_size;
  return true;
}

bool OutputStreamTestHelper::Flush() {
  flush_called_ = true;
  return true;
}

void* OutputStreamTestHelper::GetData() const {
  return static_cast<void*>(data_.get());
}

void* OutputStreamTestHelper::GetAllData() const {
  return static_cast<void*>(all_data_.get());
}

void OutputStreamTestHelper::Reset() {
  data_.reset();
  size_ = 0;
  write_called_ = false;
  flush_called_ = false;
}

}  // namespace crashpad
