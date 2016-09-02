// Copyright 2016 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_UTIL_MISC_METRICS_H_
#define CRASHPAD_UTIL_MISC_METRICS_H_

#include "base/macros.h"
#include "util/file/file_io.h"

namespace crashpad {

//! \brief Container class to hold shared UMA metrics integration points.
//!
//! Each static function in this class will call a `UMA_*` from
//! `base/metrics/histogram_macros.h`. When building Crashpad standalone,
//! (against mini_chromium), these macros do nothing. When built against
//! Chromium's base, they allow integration with its metrics system.
class Metrics {
 public:
  //! \brief Reports the size of a crash report file in bytes. Should be called
  //!     when a new report is written to disk.
  static void CrashReportSize(FileHandle file);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(Metrics);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_MISC_METRICS_H_
