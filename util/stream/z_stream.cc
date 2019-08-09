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

#include "util/stream/z_stream.h"

#include "base/logging.h"

namespace crashpad {

ZStream::ZStream(std::unique_ptr<OutputStream> output_stream, Mode mode)
    : OutputStream(std::move(output_stream)), mode_(mode) {}

ZStream::~ZStream() {
  if (!init_)
    return;
  if (mode_ == Mode::kDeflate)
    deflateEnd(&strm_);
  else if (mode_ == Mode::kInflate)
    inflateEnd(&strm_);
}

bool ZStream::Write(const void* data, size_t size) {
  if (!Init())
    return false;
  if (mode_ == Mode::kDeflate)
    return Deflate(data, size);
  else if (mode_ == Mode::kInflate)
    return Inflate(data, size);

  return false;
}

bool ZStream::Flush() {
  bool ret = false;
  if (mode_ == Mode::kDeflate)
    ret = Deflate();
  else if (mode_ == Mode::kInflate)
    ret = Inflate();

  if (!ret)
    return false;
  return output_stream_->Flush();
}

bool ZStream::Init() {
  if (init_)
    return true;

  init_ = true;
  memset(&strm_, 0, sizeof(strm_));
  int res;
  if (mode_ == Mode::kInflate) {
    res = inflateInit(&strm_);
  } else if (mode_ == Mode::kDeflate) {
    res = deflateInit(&strm_, Z_BEST_COMPRESSION);
  } else {
    NOTREACHED();
    return false;
  }

  if (res != Z_OK) {
    LOG(ERROR) << "Init: " << strm_.msg;
    return false;
  }
  strm_.next_out = buffer_;
  strm_.avail_out = kBufferLen;
  return true;
}

bool ZStream::Inflate(const void* data, size_t size) {
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

bool ZStream::Inflate() {
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

bool ZStream::Deflate(const void* data, size_t size) {
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

bool ZStream::Deflate() {
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

bool ZStream::WriteOutputStream() {
  auto valid_size = kBufferLen - strm_.avail_out;
  if (valid_size > 0) {
    if (!output_stream_->Write(buffer_, valid_size))
      return false;
  }
  strm_.next_out = buffer_;
  strm_.avail_out = kBufferLen;

  return true;
}

}  // namespace crashpad
