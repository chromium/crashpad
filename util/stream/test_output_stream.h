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

#ifndef CRASHPAD_UTIL_STREAM_TEST_OUTPUT_STREAM_H_
#define CRASHPAD_UTIL_STREAM_TEST_OUTPUT_STREAM_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "util/stream/output_stream_interface.h"

namespace crashpad {

//! \brief The help class for \a OutputStreamInterface related tests.
class TestOutputStream : public OutputStreamInterface {
 public:
  TestOutputStream();
  ~TestOutputStream() override;

  // OutputStreamInterface:
  bool Write(const uint8_t* data, size_t size) override;
  bool Flush() override;

  //! \brief Gets the data that has been received by the last call of Write().
  const std::vector<uint8_t>& last_written_data() const {
    return last_written_data_;
  }

  //! \brief Gets all data that has been received.
  const std::vector<uint8_t>& all_data() const { return all_data_; }

  //! \brief Returns true if Write() has been called.
  bool write_called() const { return write_called_; }

  //! \brief Returns true if Flush() has been called.
  bool flush_called() const { return flush_called_; }

  //! \brief Resets all internal state except |all_data_| and |total_size_|.
  void Reset();

 private:
  std::vector<uint8_t> last_written_data_;
  std::vector<uint8_t> all_data_;
  bool write_called_;
  bool flush_called_;

  DISALLOW_COPY_AND_ASSIGN(TestOutputStream);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_STREAM_TEST_OUTPUT_STREAM_H_
