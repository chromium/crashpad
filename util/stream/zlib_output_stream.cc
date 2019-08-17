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

#include "util/stream/zlib_output_stream.h"

#include "base/logging.h"

namespace crashpad {

ZlibOutputStream::ZlibOutputStream(
    std::unique_ptr<OutputStreamInterface> output_stream,
    Mode mode)
    : OutputStreamInterface(std::move(output_stream)),
      mode_(mode),
      initialized_(false),
      flush_needed_(false) {}

ZlibOutputStream::~ZlibOutputStream() {
  if (!initialized_)
    return;
  DCHECK(!flush_needed_);
  if (mode_ == Mode::kCompress) {
    if (Z_OK != deflateEnd(&zlib_stream_))
      LOG(ERROR) << "deflatEnd error: " << zlib_stream_.msg;
  } else if (mode_ == Mode::kDecompress) {
    if (Z_OK != inflateEnd(&zlib_stream_))
      LOG(ERROR) << "inflatEnd error: " << zlib_stream_.msg;
  }
}

bool ZlibOutputStream::Write(const void* data, size_t size) {
  if (!initialized_) {
    if (!Initialize())
      return false;
  }

  zlib_stream_.next_in = static_cast<unsigned char*>(const_cast<void*>(data));
  zlib_stream_.avail_in = size;
  flush_needed_ = false;
  while (zlib_stream_.avail_in > 0) {
    if (mode_ == Mode::kCompress) {
      if (Z_OK != deflate(&zlib_stream_, Z_NO_FLUSH)) {
        LOG(ERROR) << "deflat error: " << zlib_stream_.msg;
        return false;
      }
    } else if (mode_ == Mode::kDecompress) {
      int result = inflate(&zlib_stream_, Z_NO_FLUSH);
      if (result != Z_OK && result != Z_STREAM_END) {
        LOG(ERROR) << "inflat error: " << zlib_stream_.msg;
        return false;
      }
    }

    if (!WriteOutputStream())
      return false;
  }
  flush_needed_ = true;
  return true;
}

bool ZlibOutputStream::Flush() {
  flush_needed_ = false;
  int result = Z_OK;
  while (result != Z_STREAM_END) {
    if (mode_ == Mode::kCompress) {
      result = deflate(&zlib_stream_, Z_FINISH);
      if (result != Z_STREAM_END && result != Z_BUF_ERROR && result != Z_OK) {
        LOG(ERROR) << "deflat error: " << zlib_stream_.msg;
        return false;
      }
    } else if (mode_ == Mode::kDecompress) {
      result = inflate(&zlib_stream_, Z_FINISH);
      if (result != Z_STREAM_END && result != Z_BUF_ERROR) {
        LOG(ERROR) << "inflat error: " << zlib_stream_.msg;
        return false;
      }
    }
    if (!WriteOutputStream())
      return false;
  }
  return output_stream_->Flush();
}

bool ZlibOutputStream::Initialize() {
  memset(&zlib_stream_, 0, sizeof(zlib_stream_));
  if (mode_ == Mode::kDecompress) {
    int result = inflateInit(&zlib_stream_);
    if (result != Z_OK) {
      LOG(ERROR) << "inflateInit error " << result;
      return false;
    }
  } else if (mode_ == Mode::kCompress) {
    int result = deflateInit(&zlib_stream_, Z_BEST_COMPRESSION);
    if (result != Z_OK) {
      LOG(ERROR) << "deflateInit error " << result;
      return false;
    }
  }
  zlib_stream_.next_out = buffer_;
  zlib_stream_.avail_out = base::size(buffer_);
  initialized_ = true;
  return true;
}

bool ZlibOutputStream::WriteOutputStream() {
  auto valid_size = base::size(buffer_) - zlib_stream_.avail_out;
  if (valid_size > 0) {
    if (!output_stream_->Write(buffer_, valid_size))
      return false;
  }
  zlib_stream_.next_out = buffer_;
  zlib_stream_.avail_out = base::size(buffer_);

  return true;
}

}  // namespace crashpad
