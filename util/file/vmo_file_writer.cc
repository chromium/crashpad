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

#include "util/file/vmo_file_writer.h"

#include "base/logging.h"

#include <stdio.h>

namespace crashpad {

namespace {

int VerifyBounds(int base, int offset, int max) {
  int pos = base + offset;
  if (pos < 0) {
    PLOG(ERROR) << "Seek: Offset before beginning of file: " << pos;
    return -1;
  }

  if (pos > max) {
    PLOG(ERROR) << "Seek: Offset beyond EOF: " << pos;
    return -1;
  }

  return 0;
}

}  // namespace

// FileSeekerInterface Implementation --------------------------------------------------------------

FileOffset VMOFileWriter::Seek(FileOffset offset, int whence) {
  switch (whence) {
    // Beginning of file.
    case SEEK_SET: {
      if (VerifyBounds(0, offset, data_.size()) == -1)
        return -1;

      offset_ = offset;
      return offset_;
    }
    case SEEK_CUR: {
      if (VerifyBounds(offset_, offset, data_.size()) == -1)
        return -1;
      offset_ += offset;
      return offset_;
    }
    case SEEK_END: {
      int cur = data_.size();
      if (VerifyBounds(cur, offset, data_.size()) == -1)
        return -1;
      offset_ = cur + offset;
      return offset_;
    }
    default: {
      PLOG(ERROR) << "Seek: wrong whence value: " << whence;
      return -1;
    }
  }

  return 0;
}

// FileWriterInterface Implementation --------------------------------------------------------------

VMOFileWriter::~VMOFileWriter() = default;

bool VMOFileWriter::Write(const void* input, size_t size) {
  uint8_t* data_begin = data_.data();
  uint8_t* data_end = data_begin + data_.size();

  const uint8_t* input_begin = reinterpret_cast<const uint8_t*>(input);
  const uint8_t* input_end = input_begin + size;

  const uint8_t* input_ptr = input_begin;
  uint8_t* data_ptr = data_begin + offset_;
  while (input_ptr < input_end && data_ptr < data_end) {
    *data_ptr++ = *input_ptr++;
    offset_++;
  }

  // If we wrote the whole input, we're done.
  if (input_ptr == input_end)
    return true;

  // Otherwise we reached the EOF and need to make the backing vector grow.
  CHECK_EQ(data_ptr, data_end);
  data_.insert(data_.end(), input_ptr, input_end);
  offset_ = data_.size();

  return true;
}

bool VMOFileWriter::WriteIoVec(std::vector<crashpad::WritableIoVec>* iovecs) {
  for (auto& iovec : *iovecs) {
    if (!Write(iovec.iov_base, iovec.iov_len))
      return false;
  }

  return true;
}

// VMOFileWriter Implementation ------------------------------------------------------------------------

zx_status_t VMOFileWriter::GenerateVMO(zx::vmo* out) const {
  zx::vmo vmo;
  zx_status_t status = zx::vmo::create(data_.size(), 0, &vmo);
  if (status != ZX_OK) {
    PLOG(ERROR) << "Could create VMO.";
    return status;
  }

  // Write the dat into the vmo.
  status = vmo.write(data_.data(), 0, data_.size());
  if (status != ZX_OK) {
    PLOG(ERROR) << "Could not write into the VMO.";
    return status;
  }

  *out = std::move(vmo);
  return ZX_OK;
}

}  // namespace crashpad
