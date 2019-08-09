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

#ifndef CRASHPAD_UTIL_STREAM_Z_STREAM_H_
#define CRASHPAD_UTIL_STREAM_Z_STREAM_H_

#include "base/gtest_prod_util.h"
#include "third_party/zlib/zlib_crashpad.h"
#include "util/stream/output_stream.h"

namespace crashpad {

//! \brief The class wraps zlib into \a OutputStream.
class ZStream : public OutputStream {
 public:
  //! \brief The work mode of this object.
  enum class Mode : bool {
    //! \brief Compress the data from \a Write with the best compression.
    kDeflate = false,
    //! \brief Uncompress the data from \a Write.
    kInflate = true
  };
  //! \brief construct a ZStream object.
  //!
  //! \param[in] mode The work mode this object .
  ZStream(std::unique_ptr<OutputStream> output_stream, Mode mode);
  ~ZStream();

  // OutputStream
  bool Write(const void* data, size_t size) override;
  bool Flush() override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ZStreamTest, WriteLongDataOneTime);
  FRIEND_TEST_ALL_PREFIXES(ZStreamTest, WriteLongDataMultipleTimes);
  bool Init();
  bool Inflate(const void* data, size_t size);
  bool Inflate();
  bool Deflate(const void* data, size_t size);
  bool Deflate();
  bool WriteOutputStream();

  static constexpr size_t kBufferLen = 2048;

  uint8_t buffer_[kBufferLen];
  z_stream strm_;
  Mode mode_;
  bool init_ = false;

  DISALLOW_COPY_AND_ASSIGN(ZStream);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_STREAM_Z_STREAM_H_
