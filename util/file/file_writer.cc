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

#include "util/file/file_writer.h"

#include <algorithm>

#include <limits.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "util/file/fd_io.h"

namespace crashpad {

// Ensure type compatibility between WritableIoVec and iovec.
static_assert(sizeof(WritableIoVec) == sizeof(iovec), "WritableIoVec size");
static_assert(offsetof(WritableIoVec, iov_base) == offsetof(iovec, iov_base),
              "WritableIoVec base offset");
static_assert(offsetof(WritableIoVec, iov_len) == offsetof(iovec, iov_len),
              "WritableIoVec len offset");

FileWriter::FileWriter() : fd_() {
}

FileWriter::~FileWriter() {
}

bool FileWriter::Open(const base::FilePath& path, int oflag, mode_t mode) {
  CHECK(!fd_.is_valid());

  DCHECK((oflag & O_WRONLY) || (oflag & O_RDWR));

  fd_.reset(HANDLE_EINTR(open(path.value().c_str(), oflag, mode)));
  if (!fd_.is_valid()) {
    PLOG(ERROR) << "open " << path.value();
    return false;
  }

  return true;
}

void FileWriter::Close() {
  CHECK(fd_.is_valid());

  fd_.reset();
}

bool FileWriter::Write(const void* data, size_t size) {
  DCHECK(fd_.is_valid());

  // TODO(mark): Write no more than SSIZE_MAX bytes in a single call.
  ssize_t written = WriteFD(fd_.get(), data, size);
  if (written < 0) {
    PLOG(ERROR) << "write";
    return false;
  } else if (written == 0) {
    LOG(ERROR) << "write: returned 0";
    return false;
  }

  return true;
}

bool FileWriter::WriteIoVec(std::vector<WritableIoVec>* iovecs) {
  DCHECK(fd_.is_valid());

  ssize_t size = 0;
  for (const WritableIoVec& iov : *iovecs) {
    // TODO(mark): Check to avoid overflow of ssize_t, and fail with EINVAL.
    size += iov.iov_len;
  }

  // Get an iovec*, because that’s what writev wants. The only difference
  // between WritableIoVec and iovec is that WritableIoVec’s iov_base is a
  // pointer to a const buffer, where iovec’s iov_base isn’t. writev doesn’t
  // actually write to the data, so this cast is safe here. iovec’s iov_base is
  // non-const because the same structure is used for readv and writev, and
  // readv needs to write to the buffer that iov_base points to.
  iovec* iov = reinterpret_cast<iovec*>(&(*iovecs)[0]);
  size_t remaining_iovecs = iovecs->size();

  while (size > 0) {
    size_t writev_iovec_count =
        std::min(remaining_iovecs, implicit_cast<size_t>(IOV_MAX));
    ssize_t written = HANDLE_EINTR(writev(fd_.get(), iov, writev_iovec_count));
    if (written < 0) {
      PLOG(ERROR) << "writev";
      return false;
    } else if (written == 0) {
      LOG(ERROR) << "writev: returned 0";
      return false;
    }

    size -= written;
    DCHECK_GE(size, 0);

    if (size == 0) {
      remaining_iovecs = 0;
      break;
    }

    while (written > 0) {
      size_t wrote_this_iovec =
          std::min(implicit_cast<size_t>(written), iov->iov_len);
      written -= wrote_this_iovec;
      if (wrote_this_iovec < iov->iov_len) {
        iov->iov_base =
            reinterpret_cast<char*>(iov->iov_base) + wrote_this_iovec;
        iov->iov_len -= wrote_this_iovec;
      } else {
        ++iov;
        --remaining_iovecs;
      }
    }
  }

  DCHECK_EQ(remaining_iovecs, 0u);

#ifndef NDEBUG
  // The interface says that |iovecs| is not sacred, so scramble it to make sure
  // that nobody depends on it.
  memset(&(*iovecs)[0], 0xa5, sizeof((*iovecs)[0]) * iovecs->size());
#endif

  return true;
}

off_t FileWriter::Seek(off_t offset, int whence) {
  DCHECK(fd_.is_valid());

  off_t rv = lseek(fd_.get(), offset, whence);
  if (rv < 0) {
    PLOG(ERROR) << "lseek";
  }

  return rv;
}

}  // namespace crashpad
