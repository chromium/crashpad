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

#include "util/misc/metrics.h"

#include "base/metrics/histogram_macros.h"

namespace crashpad {

void Metrics::CrashReportSize(FileHandle file) {
  const FileOffset size = LoggingFileSizeByHandle(file);
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "Crashpad.CrashReportSize", size, 0, 5 * 1024 * 1024, 50);
}

}  // namespace crashpad
