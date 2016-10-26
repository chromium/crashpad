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

#include "client/crash_report_database.h"

namespace crashpad {

CrashReportDatabase::Report::Report()
    : uuid(),
      file_path(),
      id(),
      creation_time(0),
      uploaded(false),
      last_upload_attempt_time(0),
      upload_attempts(0),
      upload_explicitly_requested(false) {}

CrashReportDatabase::CallErrorWritingCrashReport::CallErrorWritingCrashReport(
    CrashReportDatabase* database,
    NewReport* new_report)
    : database_(database),
      new_report_(new_report) {
}

CrashReportDatabase::CallErrorWritingCrashReport::
    ~CallErrorWritingCrashReport() {
  if (new_report_) {
    database_->ErrorWritingCrashReport(new_report_);
  }
}

void CrashReportDatabase::CallErrorWritingCrashReport::Disarm() {
  new_report_ = nullptr;
}

}  // namespace crashpad
