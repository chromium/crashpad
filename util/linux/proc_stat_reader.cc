// Copyright 2017 The Crashpad Authors. All rights reserved.
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

#include "util/linux/proc_stat_reader.h"

#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "util/file/file_io.h"
#include "util/misc/lexing.h"

namespace crashpad {

namespace {

void SubtractTimespec(const timespec& t1,
                      const timespec& t2,
                      timespec* result) {
  result->tv_sec = t1.tv_sec - t2.tv_sec;
  result->tv_nsec = t1.tv_nsec - t2.tv_nsec;
  if (result->tv_nsec < 0) {
    result->tv_sec -= 1;
    result->tv_nsec += static_cast<long>(1E9);
  }
}

void TimespecToTimeval(const timespec& ts, timeval* tv) {
  tv->tv_sec = ts.tv_sec;
  tv->tv_usec = ts.tv_nsec / 1000;
}

}  // namespace

ProcStatReader::ProcStatReader() : tid_(-1) {}

ProcStatReader::~ProcStatReader() {}

bool ProcStatReader::Initialize(pid_t tid) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);
  // This might do more in the future.
  tid_ = tid;
  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

bool ProcStatReader::StartTime(timeval* start_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  std::string stat_contents;
  if (!ReadFile(&stat_contents)) {
    return false;
  }

  // The process start time is the 22nd column.
  // The second column is the executable name in parentheses.
  // The executable name may have parentheses itself, so find the end of the
  // second column by working backwards to find the last closing parens and
  // then count forward to the 22nd column.
  size_t stat_pos = stat_contents.rfind(')');
  if (stat_pos == std::string::npos) {
    LOG(ERROR) << "format error";
    return false;
  }

  for (int index = 1; index < 21; ++index) {
    stat_pos = stat_contents.find(' ', stat_pos);
    if (stat_pos == std::string::npos) {
      break;
    }
    ++stat_pos;
  }
  if (stat_pos >= stat_contents.size()) {
    LOG(ERROR) << "format error";
    return false;
  }

  const char* ticks_ptr = &stat_contents[stat_pos];

  // start time is in jiffies instead of clock ticks pre 2.6.
  uint64_t ticks_after_boot;
  if (!AdvancePastNumber<uint64_t>(&ticks_ptr, &ticks_after_boot)) {
    LOG(ERROR) << "format error";
    return false;
  }
  long clock_ticks_per_s = sysconf(_SC_CLK_TCK);
  if (clock_ticks_per_s <= 0) {
    PLOG(ERROR) << "sysconf";
    return false;
  }
  timeval time_after_boot;
  time_after_boot.tv_sec = ticks_after_boot / clock_ticks_per_s;
  time_after_boot.tv_usec = (ticks_after_boot % clock_ticks_per_s) *
                            (static_cast<long>(1E6) / clock_ticks_per_s);

  timespec uptime;
  if (clock_gettime(CLOCK_BOOTTIME, &uptime) != 0) {
    PLOG(ERROR) << "clock_gettime";
    return false;
  }

  timespec current_time;
  if (clock_gettime(CLOCK_REALTIME, &current_time) != 0) {
    PLOG(ERROR) << "clock_gettime";
    return false;
  }

  timespec boot_time_ts;
  SubtractTimespec(current_time, uptime, &boot_time_ts);
  timeval boot_time_tv;
  TimespecToTimeval(boot_time_ts, &boot_time_tv);
  timeradd(&boot_time_tv, &time_after_boot, start_time);

  return true;
}

bool ProcStatReader::ReadFile(std::string* contents) const {
  char path[32];
  snprintf(path, arraysize(path), "/proc/%d/stat", tid_);
  if (!LoggingReadEntireFile(base::FilePath(path), contents)) {
    return false;
  }
  return true;
}

}  // namespace crashpad
