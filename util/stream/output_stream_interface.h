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

#ifndef CRASHPAD_UTIL_STREAM_OUTPUT_STREAM_INTERFACE_H_
#define CRASHPAD_UTIL_STREAM_OUTPUT_STREAM_INTERFACE_H_

#include <memory>

#include "base/macros.h"

namespace crashpad {

//! \brief The interface for an output stream pipeline.
//!
//! Example:
//! <code>
//!   class OutputStreamInterfaceImpl : public OutputStreamInterface {
//!     ...
//!   };
//!
//!   // Create a OutputStream.
//!   OutputStreamInterfaceImpl impl(...);
//!   // Write the data multiple times.
//!   while (has_data) {
//!     impl.Write(data, size);
//!     ...
//!   }
//!   // Flush internal buffer to indicate all data has been written.
//!   impl.Flush();
//! </code>
//!
class OutputStreamInterface {
 public:
  //! \param[in] output_stream The output_stream, if specified, is stream that
  //! this object writes to.
  //!
  //! To construct an output pipeline, normally, the output stream need an
  //! output stream to write the result to, for example, below code constructs
  //! a compress->base94-encoding->log output stream pipline.
  //!
  //! <code>
  //!   ZlibOutputStream zlib_output_stream(
  //!        std::make_unique<Base94OutputStream>(
  //!            std::make_unique<LogOutputStream>(),
  //!            Base94OutputStream::Mode::kEncode),
  //!        ZlibOutputStream::Mode::kDeflate);
  //! </code>
  //!
  //! Only the last stream in pipline doesn't need OutputStream.
  //!
  explicit OutputStreamInterface(
      std::unique_ptr<OutputStreamInterface> output_stream)
      : output_stream_(std::move(output_stream)) {}
  virtual ~OutputStreamInterface() = default;

  //! \brief Writes \a data to this stream. This method may be called multiple
  //! times for streaming.
  //!
  //! \param[in] data The data that should be written.
  //! \param[in] size The size of \a data.
  //!
  //! \return `true` on success.
  virtual bool Write(const void* data, size_t size) = 0;

  //! \brief Flush the internal buffer after all data has been written.
  //!
  //! Write() can't be called afterwards.
  //! \return `true` on success.
  virtual bool Flush() = 0;

  OutputStreamInterface* GetOutputStreamForTesting() const {
    return output_stream_.get();
  }

 protected:
  std::unique_ptr<OutputStreamInterface> output_stream_;

  DISALLOW_COPY_AND_ASSIGN(OutputStreamInterface);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_STREAM_OUTPUT_STREAM_INTERFACE_H_
