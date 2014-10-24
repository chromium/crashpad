// Copyright 2014 The Crashpad Authors. All rights reserved.
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

#include "util/net/http_body.h"

#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <limits>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/stl_util.h"
#include "util/file/fd_io.h"

namespace crashpad {

StringHTTPBodyStream::StringHTTPBodyStream(const std::string& string)
    : HTTPBodyStream(), string_(string), bytes_read_() {
}

StringHTTPBodyStream::~StringHTTPBodyStream() {
}

ssize_t StringHTTPBodyStream::GetBytesBuffer(uint8_t* buffer, size_t max_len) {
  size_t num_bytes_remaining = string_.length() - bytes_read_;
  if (num_bytes_remaining == 0) {
    return num_bytes_remaining;
  }

  size_t num_bytes_returned =
      std::min(std::min(num_bytes_remaining, max_len),
               static_cast<size_t>(std::numeric_limits<ssize_t>::max()));
  memcpy(buffer, &string_[bytes_read_], num_bytes_returned);
  bytes_read_ += num_bytes_returned;
  return num_bytes_returned;
}

FileHTTPBodyStream::FileHTTPBodyStream(const base::FilePath& path)
    : HTTPBodyStream(), path_(path), fd_(kUnopenedFile) {
}

FileHTTPBodyStream::~FileHTTPBodyStream() {
  if (fd_ >= 0) {
    LoggingCloseFD(fd_);
  }
}

ssize_t FileHTTPBodyStream::GetBytesBuffer(uint8_t* buffer, size_t max_len) {
  switch (fd_) {
    case kUnopenedFile:
      fd_ = HANDLE_EINTR(open(path_.value().c_str(), O_RDONLY));
      if (fd_ < 0) {
        fd_ = kFileOpenError;
        PLOG(ERROR) << "Cannot open " << path_.value();
        return -1;
      }
      break;
    case kFileOpenError:
      return -1;
    case kClosedAtEOF:
      return 0;
    default:
      break;
  }

  ssize_t rv = ReadFD(fd_, buffer, max_len);
  if (rv == 0) {
    LoggingCloseFD(fd_);
    fd_ = kClosedAtEOF;
  } else if (rv < 0) {
    PLOG(ERROR) << "read";
  }
  return rv;
}

CompositeHTTPBodyStream::CompositeHTTPBodyStream(
    const CompositeHTTPBodyStream::PartsList& parts)
    : HTTPBodyStream(), parts_(parts), current_part_(parts_.begin()) {
}

CompositeHTTPBodyStream::~CompositeHTTPBodyStream() {
  STLDeleteContainerPointers(parts_.begin(), parts_.end());
}

ssize_t CompositeHTTPBodyStream::GetBytesBuffer(uint8_t* buffer,
                                                size_t max_len) {
  if (current_part_ == parts_.end())
    return 0;

  ssize_t rv = (*current_part_)->GetBytesBuffer(buffer, max_len);

  if (rv == 0) {
    // If the current part has returned 0 indicating EOF, advance the current
    // part and call recursively to try again.
    ++current_part_;
    return GetBytesBuffer(buffer, max_len);
  }

  return rv;
}

}  // namespace crashpad
