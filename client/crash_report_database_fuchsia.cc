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

#include "client/crash_report_database.h"

#include "base/logging.h"

namespace crashpad {

// static
std::unique_ptr<CrashReportDatabase> CrashReportDatabase::Initialize(
    const base::FilePath& path) {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
  return std::unique_ptr<CrashReportDatabase>();
}

// static
std::unique_ptr<CrashReportDatabase>
CrashReportDatabase::InitializeWithoutCreating(const base::FilePath& path) {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
  return std::unique_ptr<CrashReportDatabase>();
}

}  // namespace crashpad
