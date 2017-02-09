// Copyright 2017 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_UTIL_NET_HTTP_BODY_GZIP_H_
#define CRASHPAD_UTIL_NET_HTTP_BODY_GZIP_H_

#include <stdint.h>
#include <sys/types.h>

#include <memory>

#include "base/macros.h"
#include "third_party/zlib/zlib.h"
#include "util/file/file_io.h"
#include "util/net/http_body.h"

namespace crashpad {

class GzipHTTPBodyStream : public HTTPBodyStream {
 public:
  explicit GzipHTTPBodyStream(std::unique_ptr<HTTPBodyStream> source);

  ~GzipHTTPBodyStream() override;

  // HTTPBodyStream:
  FileOperationResult GetBytesBuffer(uint8_t* buffer, size_t max_len) override;

 private:
  enum State : int {
    kUninitialized,
    kOperating,
    kInputEOF,
    kFinished,
    kError,
  };

  uint8_t input_[4096];
  z_stream z_stream_;
  std::unique_ptr<HTTPBodyStream> source_;
  State state_;

  DISALLOW_COPY_AND_ASSIGN(GzipHTTPBodyStream);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_NET_HTTP_BODY_GZIP_H_
