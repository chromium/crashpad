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
      flushed_(false) {}

ZlibOutputStream::~ZlibOutputStream() {
  if (!initialized_.is_valid())
    return;
  DCHECK(flushed_);
  if (mode_ == Mode::kCompress)
    deflateEnd(&strm_);
  else if (mode_ == Mode::kDecompress)
    inflateEnd(&strm_);
}

bool ZlibOutputStream::Write(const void* data, size_t size) {
  if (initialized_.is_uninitialized())
    Initialize();

  if (!initialized_.is_valid())
    return false;

  if (mode_ == Mode::kCompress)
    return Deflate(data, size);
  else if (mode_ == Mode::kDecompress)
    return Inflate(data, size);

  return false;
}

bool ZlibOutputStream::Flush() {
  DCHECK(initialized_.is_valid());
  flushed_ = true;
  bool ret = false;
  if (mode_ == Mode::kCompress)
    ret = Deflate();
  else if (mode_ == Mode::kDecompress)
    ret = Inflate();

  if (!ret)
    return false;
  return output_stream_->Flush();
}

void ZlibOutputStream::Initialize() {
  memset(&strm_, 0, sizeof(strm_));
  int res;
  initialized_.set_invalid();
  if (mode_ == Mode::kDecompress) {
    res = inflateInit(&strm_);
  } else if (mode_ == Mode::kCompress) {
    res = deflateInit(&strm_, Z_BEST_COMPRESSION);
  } else {
    NOTREACHED();
    return;
  }

  if (res != Z_OK) {
    LOG(ERROR) << "Initialize: " << strm_.msg;
    return;
  }
  strm_.next_out = buffer_.data();
  strm_.avail_out = buffer_.size();
  initialized_.set_valid();
  return;
}

bool ZlibOutputStream::Inflate(const void* data, size_t size) {
  strm_.next_in = (unsigned char*)data;
  strm_.avail_in = size;

  while (strm_.avail_in > 0) {
    int res = inflate(&strm_, Z_NO_FLUSH);
    if (Z_OK != res && Z_STREAM_END != res)
      return false;

    if (!WriteOutputStream())
      return false;
  }
  return true;
}

bool ZlibOutputStream::Inflate() {
  int res = Z_OK;
  while (res != Z_STREAM_END) {
    res = inflate(&strm_, Z_FINISH);
    if (res != Z_STREAM_END && res != Z_BUF_ERROR)
      return false;

    if (!WriteOutputStream())
      return false;
  }

  return true;
}

bool ZlibOutputStream::Deflate(const void* data, size_t size) {
  strm_.next_in = (unsigned char*)data;
  strm_.avail_in = size;

  while (strm_.avail_in > 0) {
    if (Z_OK != deflate(&strm_, Z_NO_FLUSH))
      return false;

    if (!WriteOutputStream())
      return false;
  }
  return true;
}

bool ZlibOutputStream::Deflate() {
  int res = Z_OK;
  while (res != Z_STREAM_END) {
    res = deflate(&strm_, Z_FINISH);
    if (res != Z_STREAM_END && res != Z_BUF_ERROR && res != Z_OK)
      return false;

    if (!WriteOutputStream())
      return false;
  }
  return true;
}

bool ZlibOutputStream::WriteOutputStream() {
  auto valid_size = buffer_.size() - strm_.avail_out;
  if (valid_size > 0) {
    if (!output_stream_->Write(buffer_.data(), valid_size))
      return false;
  }
  strm_.next_out = buffer_.data();
  strm_.avail_out = buffer_.size();

  return true;
}

}  // namespace crashpad
