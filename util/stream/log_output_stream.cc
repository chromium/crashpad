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

#include <algorithm>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"

#if defined(OS_ANDROID)
#include <android/log.h>
#endif

namespace crashpad {

namespace {

// Most minidumps are expected to be compressed and encoded into less than 128k.
constexpr size_t kOutputCap = 128 * 1024;

// From Android NDK 20r android/log.h, log message text may be truncated to
// less than an implementation-specific limit (1023 bytes), for sake of safe
// and easy to read in logcat, we choose 512.
constexpr size_t kLineBufferSize = 512;

#if defined(OS_ANDROID)
const char kAndroidLogTag[] = "crashpad";
#endif
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
  flush_needed_ = true;
  while (size > 0) {
    size_t m = std::min(kLineBufferSize - buffer_.size(), size);
    buffer_.append(reinterpret_cast<const char*>(data), m);
    data += m;
    size -= m;
    if (buffer_.size() == kLineBufferSize && !WriteBuffer()) {
      flush_needed_ = false;
      LOG(ERROR) << "Write: exceeds cap.";
      if (output_stream_for_testing_)
        output_stream_for_testing_->Flush();
      return false;
    }
  }
  return true;
}

bool LogOutputStream::WriteBuffer() {
  if (output_count_ == 0)
    WriteToLog("-----BEGIN CRASHPAD MINIDUMP-----");

  if (buffer_.empty())
    return true;

  output_count_ += buffer_.size();
  if (output_count_ > kOutputCap) {
    WriteToLog("-----ABORT CRASHPAD MINIDUMP-----");
    return false;
  }

  WriteToLog(buffer_.data());
  buffer_.clear();
  return true;
}

void LogOutputStream::WriteToLog(const char* buf) {
#if defined(OS_ANDROID)
  __android_log_buf_write(LOG_ID_CRASH, ANDROID_LOG_FATAL, kAndroidLogTag, buf);
#endif
  // For testing.
  if (output_stream_for_testing_) {
    output_stream_for_testing_->Write(reinterpret_cast<const uint8_t*>(buf),
                                      strlen(buf));
  }
}

bool LogOutputStream::Flush() {
  flush_needed_ = false;
  flushed_ = true;

  bool result = true;
  if (WriteBuffer()) {
    WriteToLog("-----END CRASHPAD MINIDUMP-----");
  } else {
    LOG(ERROR) << "Flush: exceeds cap.";
    result = false;
  }

  // Since output_stream_for_testing_'s Write() method has been called, its
  // Flush() shall always be invoked.
  if (output_stream_for_testing_)
    output_stream_for_testing_->Flush();

  return result;
}

}  // namespace crashpad
