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

#include "util/stream/log_output_stream.h"

#include <string.h>

#include <algorithm>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"

#if defined(OS_ANDROID)
#include <android/log.h>
#endif

namespace crashpad {

namespace {

// Most minidumps are expected to be compressed and encoded into less than 128k.
constexpr size_t kOutputCap = 128 * 1024;

// From Android NDK r20 <android/log.h>, log message text may be truncated to
// less than an implementation-specific limit (1023 bytes), for sake of safe
// and being easy to read in logcat, choose 512.
constexpr size_t kLineBufferSize = 512;

}  // namespace

LogOutputStream::LogOutputStream()
    : output_count_(0), flush_needed_(false), flushed_(false) {
  buffer_.reserve(kLineBufferSize);
}

LogOutputStream::~LogOutputStream() {
  DCHECK(!flush_needed_);
}

bool LogOutputStream::Write(const uint8_t* data, size_t size) {
  DCHECK(!flushed_);

  static constexpr char kBeginMessage[] = "-----BEGIN CRASHPAD MINIDUMP-----";
  if (output_count_ == 0 && WriteToLog(kBeginMessage) < 0) {
    return false;
  }

  flush_needed_ = true;
  while (size > 0) {
    size_t m = std::min(kLineBufferSize - buffer_.size(), size);
    buffer_.append(reinterpret_cast<const char*>(data), m);
    data += m;
    size -= m;
    if (!WriteBuffer()) {
      return false;
    }
  }
  return true;
}

bool LogOutputStream::WriteBuffer() {
  if (buffer_.empty())
    return true;

  static constexpr char kAbortMessage[] = "-----ABORT CRASHPAD MINIDUMP-----";

  output_count_ += buffer_.size();
  if (output_count_ > kOutputCap) {
    WriteToLog(kAbortMessage);
    return false;
  }

  int result = WriteToLog(buffer_.c_str());
  if (result < 0) {
    if (result == -EAGAIN) {
      WriteToLog(kAbortMessage);
    }
    flush_needed_ = false;
    return false;
  }

  buffer_.clear();
  return true;
}

int LogOutputStream::WriteToLog(const char* buf) {
  int ret = 0;
#if defined(OS_ANDROID)
  ret =
      __android_log_buf_write(LOG_ID_CRASH, ANDROID_LOG_FATAL, "crashpad", buf);
  if (ret < 0) {
    errno = -ret;
    PLOG(ERROR) << "__android_log_buf_write";
  }
#endif
  // For testing.
  if (output_stream_for_testing_) {
    output_stream_for_testing_->Write(reinterpret_cast<const uint8_t*>(buf),
                                      strlen(buf));
  }
  return ret;
}

bool LogOutputStream::Flush() {
  bool result = true;
  if (flush_needed_) {
    flush_needed_ = false;
    flushed_ = true;

    static constexpr char kEndMessage[] = "-----END CRASHPAD MINIDUMP-----";
    if (!WriteBuffer() || WriteToLog(kEndMessage) < 0) {
      result = false;
    }
  }

  // Since output_stream_for_testing_'s Write() method has been called, its
  // Flush() shall always be invoked.
  if (output_stream_for_testing_)
    output_stream_for_testing_->Flush();

  return result;
}

void LogOutputStream::SetOutputStreamForTesting(
    std::unique_ptr<OutputStreamInterface> stream) {
  output_stream_for_testing_ = std::move(stream);
}

}  // namespace crashpad
