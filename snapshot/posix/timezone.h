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

#ifndef CRASHPAD_SNAPSHOT_POSIX_TIMEZONE_H_
#define CRASHPAD_SNAPSHOT_POSIX_TIMEZONE_H_

#include <time.h>

#include <string>

#include "snapshot/system_snapshot.h"

namespace crashpad {
namespace internal {

void TimeZone(const timeval& snapshot_time,
              SystemSnapshot::DaylightSavingTimeStatus* dst_status,
              int* standard_offset_seconds,
              int* daylight_offset_seconds,
              std::string* standard_name,
              std::string* daylight_name);

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_POSIX_TIMEZONE_H_
