// Copyright 2015 The Crashpad Authors. All rights reserved.
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

#include "util/win/time.h"

#include <stdint.h>

#include "base/logging.h"

namespace crashpad {

namespace {

constexpr uint64_t kMicrosecondsPerSecond = static_cast<uint64_t>(1E6);

uint64_t FiletimeToMicroseconds(const FILETIME& filetime) {
  uint64_t t = (static_cast<uint64_t>(filetime.dwHighDateTime) << 32) |
               filetime.dwLowDateTime;
  return t / 10;  // 100 nanosecond intervals to microseconds.
}

timeval MicrosecondsToTimeval(uint64_t microseconds) {
  timeval tv;
  tv.tv_sec = static_cast<long>(microseconds / kMicrosecondsPerSecond);
  tv.tv_usec = static_cast<long>(microseconds % kMicrosecondsPerSecond);
  return tv;
}

}  // namespace

timeval FiletimeToTimevalEpoch(const FILETIME& filetime) {
  uint64_t microseconds = FiletimeToMicroseconds(filetime);

  // Windows epoch is 1601-01-01, and FILETIME ticks are 100 nanoseconds.
  // 1601 to 1970 is 369 years + 89 leap days = 134774 days * 86400 seconds per
  // day. It's not entirely clear, but it appears that these are solar seconds,
  // not SI seconds, so there are no leap seconds to be considered.
  constexpr uint64_t kNumSecondsFrom1601To1970 = (369 * 365 + 89) * 86400ULL;
  DCHECK_GE(microseconds, kNumSecondsFrom1601To1970 * kMicrosecondsPerSecond);
  microseconds -= kNumSecondsFrom1601To1970 * kMicrosecondsPerSecond;
  return MicrosecondsToTimeval(microseconds);
}

timeval FiletimeToTimevalInterval(const FILETIME& filetime) {
  return MicrosecondsToTimeval(FiletimeToMicroseconds(filetime));
}

void GetTimeOfDay(timeval* tv) {
  FILETIME filetime;
  GetSystemTimeAsFileTime(&filetime);
  *tv = FiletimeToTimevalEpoch(filetime);
}

}  // namespace crashpad
