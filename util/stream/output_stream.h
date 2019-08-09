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

#ifndef CRASHPAD_UTIL_STREAM_OUTPUT_STREAM_H_
#define CRASHPAD_UTIL_STREAM_OUTPUT_STREAM_H_

#include <memory>

#include "base/macros.h"

namespace crashpad {

//! \brief The base class for output stream pipeline.
//!
//! Example:
//!   class OutputStreamImpl : public OutputStream {
//!     ...
//!   }
//!
//!   // Create the OutputStream.
//!   OutputStreamImpl impl(...);
//!   // Write the data multiple times.
//!   while(has_data) {
//!     impl.Write(data, size);
//!     ...
//!   }
//!   // Flush internal buffer to indicate all data has been written.
//!   impl.Flush();
//!
class OutputStream {
 public:
  //! \brief constructor
  //!
  //! \param[in] output_stream The output_stream, if specified, is stream this
  //! object writes to.
  explicit OutputStream(std::unique_ptr<OutputStream> output_stream);
  virtual ~OutputStream();

  //! \brief Writes \a data to this stream, this method could be called multiple
  //! times for streaming.
  //!
  //! \param[in] data The data should be written.
  //! \param[in] size The size of \a data.
  //!
  //! \return 'true' on success.
  virtual bool Write(const void* data, size_t size) = 0;

  //! \brief Flush the internal buffer after all data has been written.
  //!
  //! \a Write cann't be called afterwards.
  //! \return 'true' on success.
  virtual bool Flush() = 0;

  OutputStream* GetOutputStreamForTesting() const;

 protected:
  std::unique_ptr<OutputStream> output_stream_;

  DISALLOW_COPY_AND_ASSIGN(OutputStream);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_STREAM_OUTPUT_STREAM_H_
