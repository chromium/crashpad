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

#include "util/misc/time.h"

#include "build/build_config.h"

namespace crashpad {

void TimespecToTimeval(const timespec& ts, timeval* tv) {
#if defined(OS_WIN)
  tv->tv_sec = static_cast<long>(ts.tv_sec);
#else
  tv->tv_sec = ts.tv_sec;
#endif  // OS_WIN

  tv->tv_usec = ts.tv_nsec / 1000;
}

}  // namespace crashpad
