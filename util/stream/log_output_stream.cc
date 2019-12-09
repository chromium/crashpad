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
#include "util/misc/from_pointer_cast.h"

#if defined(OS_ANDROID)
#include <android/log.h>
#include <dlfcn.h>
#endif

namespace crashpad {

namespace {

// Through the experiment of 2000 samples, we should cover most of simplified
// minidump with 128k cap.
constexpr size_t kOutputCap = 128 * 1024;

// From Android system/core/liblog/README.md, one message including newlines
// shall not exceeds LOGGER_ENTRY_MAX_PAYLOAD which is defined to 4096, for
// sake of safe and easy to read in logcat, we choose 2048.
constexpr size_t kLineBufferSize = 2048;

#if defined(OS_ANDROID)
// __android_log_buf_write() is not exported in the NDK and is being used by
// dynamic runtime linking. Its declaration is taken from Android's
// system/core/include/log/log.h.
using AndroidLogBufferWriteFunc = int (*)(int bufID,
                                          int prio,
                                          const char* tag,
                                          const char* text);
const int kAndroidCrashLogId = 4;  // From LOG_ID_CRASH in log.h.
bool g_crash_log_initialized = false;
AndroidLogBufferWriteFunc g_android_log_buf_write = nullptr;
const char kAndroidLogTag[] = "crashpad";

void InitializeCrashLogWriter() {
  if (g_crash_log_initialized)
    return;
  g_android_log_buf_write = reinterpret_cast<AndroidLogBufferWriteFunc>(
      dlsym(RTLD_DEFAULT, "__android_log_buf_write"));
  g_crash_log_initialized = true;
}

#endif

}  // namespace

LogOutputStream::LogOutputStream()
    : output_count_(0), flush_needed_(false), flushed_(false) {
  buffer_.reserve(kLineBufferSize);
#if defined(OS_ANDROID)
  InitializeCrashLogWriter();
#endif
}

LogOutputStream::~LogOutputStream() {
  DCHECK(!flush_needed_);
}

bool LogOutputStream::Write(const uint8_t* data, size_t size) {
  DCHECK(!flushed_);
  flush_needed_ = true;
  while (size > 0) {
    size_t m = std::min(kLineBufferSize - buffer_.size(), size);
    buffer_.append(FromPointerCast<const char*>(data), m);
    data += m;
    size -= m;
    if (buffer_.size() == kLineBufferSize && !WriteBuffer()) {
      flush_needed_ = false;
      LOG(ERROR) << "Write: exceeds cap.";
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
  // Try writing to the crash log ring buffer. If not available, fall back to
  // the standard log buffer.
  if (g_android_log_buf_write) {
    g_android_log_buf_write(
        kAndroidCrashLogId, ANDROID_LOG_FATAL, kAndroidLogTag, buf);
    return;
  }
  __android_log_write(ANDROID_LOG_FATAL, kAndroidLogTag, buf);
#endif
  // For testing.
  if (output_stream_for_testing_) {
    output_stream_for_testing_->Write(FromPointerCast<const uint8_t*>(buf),
                                      strlen(buf));
  }
}

bool LogOutputStream::Flush() {
  flush_needed_ = false;
  flushed_ = true;

  if (!WriteBuffer()) {
    LOG(ERROR) << "Flush: exceeds cap.";
    return false;
  }

  WriteToLog("-----END CRASHPAD MINIDUMP-----");

  if (output_stream_for_testing_)
    return output_stream_for_testing_->Flush();

  return true;
}

}  // namespace crashpad
