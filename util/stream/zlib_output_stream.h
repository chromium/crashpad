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

#ifndef CRASHPAD_UTIL_STREAM_ZLIB_OUTPUT_STREAM_H_
#define CRASHPAD_UTIL_STREAM_ZLIB_OUTPUT_STREAM_H_

#include <array>

#include "base/macros.h"
#include "third_party/zlib/zlib_crashpad.h"
#include "util/misc/initialization_state.h"
#include "util/stream/output_stream_interface.h"

namespace crashpad {

//! \brief The class wraps zlib into \a OutputStreamInterface.
class ZlibOutputStream : public OutputStreamInterface {
 public:
  //! \brief Whether this object is configured to compress or decompress data.
  enum class Mode : bool {
    //! \brief Data passed through this object is compressed.
    kCompress = false,
    //! \brief Data passed through this object is decompressed.
    kDecompress = true
  };

  //! \param[in] mode The work mode of this object .
  ZlibOutputStream(std::unique_ptr<OutputStreamInterface> output_stream,
                   Mode mode);
  ~ZlibOutputStream();

  // OutputStreamInterface
  bool Write(const void* data, size_t size) override;
  bool Flush() override;

  size_t GetBufferSizeForTesting() const { return buffer_.size(); }

 private:
  // Initializes |strm_| to compress or decompress according |mode_| and sets
  // |initialized_|, caller shall check |initialized_| to see if initialization
  // is successful or not.
  void Initialize();

  // Decompresses |data| and return true if succeeded.
  bool Inflate(const void* data, size_t size);

  // Decompresses left over data in |strm_| and return true if succeeded. It
  // shall only be used by Flush().
  bool Inflate();

  // Compresses |data| and return true if succeeded.
  bool Deflate(const void* data, size_t size);

  // Compresses left over data in |strm_| and return true if succeeded. It
  // shall only be used by Flush().
  bool Deflate();

  // Write compressed/decompressed data to |output_stream_| and empty the output
  // buffer in |strm_|.
  bool WriteOutputStream();

  std::array<uint8_t, 4096> buffer_;
  z_stream strm_;
  Mode mode_;
  InitializationState initialized_;
  bool flushed_;

  DISALLOW_COPY_AND_ASSIGN(ZlibOutputStream);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_STREAM_ZLIB_OUTPUT_STREAM_H_
