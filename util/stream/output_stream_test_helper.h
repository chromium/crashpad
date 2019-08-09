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

#ifndef CRASHPAD_UTIL_STREAM_OUTPUT_STREAM_TEST_HELPER_H_
#define CRASHPAD_UTIL_STREAM_OUTPUT_STREAM_TEST_HELPER_H_

#include "util/stream/output_stream.h"

namespace crashpad {

//! \brief The help class for \a OutputStream related tests.
class OutputStreamTestHelper : public OutputStream {
 public:
  OutputStreamTestHelper();

  // OutputStream
  bool Write(const void* data, size_t size) override;
  bool Flush() override;

  //! \brief Get the data that has been received by the last call of Write().
  void* GetData() const;
  size_t size() const { return size_; }

  //! \brief Get all data has been received.
  void* GetAllData() const;
  size_t total_size() const { return total_size_; }

  bool write_called() const { return write_called_; }

  bool flush_called() const { return flush_called_; }

  void Reset();

 private:
  std::unique_ptr<uint8_t[]> data_;
  std::unique_ptr<uint8_t[]> all_data_;
  size_t size_ = 0;
  size_t total_size_ = 0;
  bool write_called_ = false;
  bool flush_called_ = false;

  DISALLOW_COPY_AND_ASSIGN(OutputStreamTestHelper);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_STREAM_OUTPUT_STREAM_TEST_HELPER_H_
