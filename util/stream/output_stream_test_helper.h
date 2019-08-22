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

#include <memory>

#include "base/macros.h"
#include "util/stream/output_stream_interface.h"

namespace crashpad {

//! \brief The help class for \a OutputStreamInterface related tests.
class OutputStreamTestHelper : public OutputStreamInterface {
 public:
  OutputStreamTestHelper();
  ~OutputStreamTestHelper() override;

  // OutputStreamInterface:
  bool Write(const uint8_t* data, size_t size) override;
  bool Flush() override;

  //! \brief Gets the data that has been received by the last call of Write().
  void* GetData() const;

  //! \brief Returns the size of data that has been received by the last call
  //! of Write().
  size_t size() const { return size_; }

  //! \brief Gets all data that has been received.
  void* GetAllData() const;

  //! \brief Gets size of all data that has been received.
  size_t total_size() const { return total_size_; }

  //! \brief Returns the times that Write() has been called.
  bool write_called() const { return write_called_; }

  //! \brief Returns the times that Flush() has been called.
  bool flush_called() const { return flush_called_; }

  //! \brief Resets all internal state except |all_data_| and |total_size_|.
  void Reset();

 private:
  std::unique_ptr<uint8_t[]> data_;
  std::unique_ptr<uint8_t[]> all_data_;
  size_t size_;
  size_t total_size_;
  bool write_called_;
  bool flush_called_;

  DISALLOW_COPY_AND_ASSIGN(OutputStreamTestHelper);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_STREAM_OUTPUT_STREAM_TEST_HELPER_H_
