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

#include "snapshot/win/process_reader_win.h"

#include "base/numerics/safe_conversions.h"
#include "util/win/time.h"

namespace crashpad {

ProcessReaderWin::ProcessReaderWin()
    : process_(INVALID_HANDLE_VALUE),
      process_info_(),
      modules_(),
      initialized_() {
}

ProcessReaderWin::~ProcessReaderWin() {
}

bool ProcessReaderWin::Initialize(HANDLE process) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  process_ = process;
  process_info_.Initialize(process);

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

bool ProcessReaderWin::ReadMemory(WinVMAddress at,
                                  WinVMSize num_bytes,
                                  void* into) {
  SIZE_T bytes_read;
  if (!ReadProcessMemory(process_,
                         reinterpret_cast<void*>(at),
                         into,
                         base::checked_cast<SIZE_T>(num_bytes),
                         &bytes_read) ||
      num_bytes != bytes_read) {
    PLOG(ERROR) << "ReadMemory at " << at << " of " << num_bytes << " failed";
    return false;
  }
  return true;
}

bool ProcessReaderWin::StartTime(timeval* start_time) const {
  FILETIME creation, exit, kernel, user;
  if (!GetProcessTimes(process_, &creation, &exit, &kernel, &user)) {
    PLOG(ERROR) << "GetProcessTimes";
    return false;
  }
  *start_time = FiletimeToTimevalEpoch(creation);
  return true;
}

bool ProcessReaderWin::CPUTimes(timeval* user_time,
                                timeval* system_time) const {
  FILETIME creation, exit, kernel, user;
  if (!GetProcessTimes(process_, &creation, &exit, &kernel, &user)) {
    PLOG(ERROR) << "GetProcessTimes";
    return false;
  }
  *user_time = FiletimeToTimevalInterval(user);
  *system_time = FiletimeToTimevalInterval(kernel);
  return true;
}

const std::vector<ProcessInfo::Module>& ProcessReaderWin::Modules() {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  if (!process_info_.Modules(&modules_)) {
    LOG(ERROR) << "couldn't retrieve modules";
  }

  return modules_;
}

}  // namespace crashpad
