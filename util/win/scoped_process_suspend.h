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

#ifndef CRASHPAD_UTIL_WIN_SCOPED_PROCESS_SUSPEND_H_
#define CRASHPAD_UTIL_WIN_SCOPED_PROCESS_SUSPEND_H_

#include <windows.h>

#include "base/macros.h"

namespace crashpad {

//! \brief Manages the suspension of another process.
//!
//! While an object of this class exists, the other process will be suspended.
//! Once the object is destroyed, the other process will become eligible for
//! resumption.
//!
//! If this process crashes while this object exists, there is no guarantee that
//! the other process will be resumed.
class ScopedProcessSuspend {
 public:
  //! Does not take ownership of \a process.
  explicit ScopedProcessSuspend(HANDLE process);
  ~ScopedProcessSuspend();

 private:
  HANDLE process_;

  DISALLOW_COPY_AND_ASSIGN(ScopedProcessSuspend);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_WIN_SCOPED_PROCESS_SUSPEND_H_
