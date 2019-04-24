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

#include "util/process/process_id.h"

#include "build/build_config.h"

namespace crashpad {

#if defined(OS_POSIX) || DOXYGEN
const ProcessID kInvalidProcessID = -1;
#elif defined(OS_WIN)
const ProcessID kInvalidProcessID = 0;
#elif defined(OS_FUCHSIA)
const ProcessID kInvalidProcessID = -1;
#else
#error Port.
#endif

}  // namespace crashpad
