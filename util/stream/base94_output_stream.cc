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

#include "util/stream/base94_output_stream.h"

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"

namespace crashpad {

namespace {
// To improve the space efficiency, we can encode 14 bits into two symbols
// if 14-bit number isn’t larger than 94^2 which is total number can be
// encoded by 2 digits of base94 number, in another word, if 13 bit number
// is smaller than 643, we could read one more bit, because even if the 14th
// bit is 1, the 14-bit number doesn’t exceed the max value.
constexpr uint16_t kMaxValueOf14BitEncoding = 643;

constexpr size_t kMaxBuffer = 4096;
}  // namespace

Base94OutputStream::Base94OutputStream(
    Mode mode,
    std::unique_ptr<OutputStreamInterface> output_stream)
    : mode_(mode),
      output_stream_(std::move(output_stream)),
      table_(),
      bit_buf_(0),
      bit_count_(0),
      symbol_buffer_(0),
      flush_needed_(false) {
  buffer_.reserve(kMaxBuffer);
}

Base94OutputStream::~Base94OutputStream() {
  DCHECK(!flush_needed_);
}

bool Base94OutputStream::Write(const uint8_t* data, size_t size) {
  if (initialized_.is_uninitialized()) {
    initialized_.set_invalid();
    if (mode_ == Mode::kEncode) {
      table_ = std::make_unique<uint8_t[]>(94);
      for (size_t index = 0; index < 94; index++)
        table_[index] = base::saturated_cast<uint8_t>(index + '!');
    } else if (mode_ == Mode::kDecode) {
      table_ = std::make_unique<uint8_t[]>(256);
      memset(table_.get(), 94, 256);
      for (size_t index = 0; index < 94; index++)
        table_[index + '!'] = base::saturated_cast<uint8_t>(index);
    }
    initialized_.set_valid();
  }

  flush_needed_ = true;
  if (mode_ == Mode::kEncode)
    return Encode(data, size);
  else
    return Decode(data, size);
}

bool Base94OutputStream::Flush() {
  if (initialized_.is_valid() && flush_needed_) {
    flush_needed_ = false;
    bool result = false;
    if (mode_ == Mode::kEncode)
      result = FinishEncoding();
    else if (mode_ == Mode::kDecode)
      result = FinishDecoding();
    if (!result)
      return false;
  }
  return output_stream_->Flush();
}

bool Base94OutputStream::Encode(const uint8_t* data, size_t size) {
  const uint8_t* cur = data;
  while (size--) {
    bit_buf_ |= *(cur++) << bit_count_;
    bit_count_ += 8;
    if (bit_count_ < 14)
      continue;

    uint16_t block;
    // Check if 13-bit or 14-bit data should be encoded.
    if ((bit_buf_ & 0x1FFF) > kMaxValueOf14BitEncoding) {
      block = bit_buf_ & 0x1FFF;
      bit_buf_ >>= 13;
      bit_count_ -= 13;
    } else {
      block = bit_buf_ & 0x3FFF;
      bit_buf_ >>= 14;
      bit_count_ -= 14;
    }
    buffer_.push_back(table_[block % 94]);
    buffer_.push_back(table_[block / 94]);

    if (buffer_.size() > kMaxBuffer) {
      if (!WriteOutputStream())
        return false;
    }
  }
  return WriteOutputStream();
}

bool Base94OutputStream::Decode(const uint8_t* data, size_t size) {
  const uint8_t* cur = data;
  while (size--) {
    if (table_[*cur] == 94) {
      LOG(ERROR) << "Decode: invalid input";
      return false;
    }
    if (symbol_buffer_ == 0) {
      symbol_buffer_ = *cur;
      cur++;
      continue;
    }
    uint16_t v = table_[symbol_buffer_] + table_[*cur] * 94;
    cur++;
    symbol_buffer_ = 0;
    bit_buf_ |= v << bit_count_;
    bit_count_ += (v & (0x1FFF)) > kMaxValueOf14BitEncoding ? 13 : 14;
    while (bit_count_ > 7) {
      buffer_.push_back(bit_buf_ & 0xff);
      bit_buf_ >>= 8;
      bit_count_ -= 8;
    }
    if (buffer_.size() > kMaxBuffer) {
      if (!WriteOutputStream())
        return false;
    }
  }
  return WriteOutputStream();
}

bool Base94OutputStream::FinishEncoding() {
  if (bit_count_ == 0)
    return true;
  // Up to 13 bits data is left over.
  buffer_.push_back(table_[bit_buf_ % 94]);
  if (bit_buf_ > 93 || bit_count_ > 8)
    buffer_.push_back(table_[bit_buf_ / 94]);
  bit_count_ = 0;
  bit_buf_ = 0;
  return WriteOutputStream();
}

bool Base94OutputStream::FinishDecoding() {
  // The left over bit is padding and all zero, if there is no symbol
  // unprocessed.
  if (symbol_buffer_ == 0) {
    DCHECK(!bit_buf_);
    return true;
  }
  bit_buf_ |= table_[symbol_buffer_] << bit_count_;
  bit_count_ += 8;
  while (bit_count_ > 7) {
    buffer_.push_back(bit_buf_ & 0xff);
    bit_buf_ >>= 8;
    bit_count_ -= 8;
  }
  DCHECK(!bit_buf_);
  return WriteOutputStream();
}

bool Base94OutputStream::WriteOutputStream() {
  if (buffer_.empty())
    return true;

  bool result = output_stream_->Write(buffer_.data(), buffer_.size());
  buffer_.clear();
  return result;
}

}  // namespace crashpad
