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

#ifndef CRASHPAD_HANDLER_FUCHSIA_CRASH_REPORT_EXCEPTION_HANDLER_H_
#define CRASHPAD_HANDLER_FUCHSIA_CRASH_REPORT_EXCEPTION_HANDLER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "client/crash_report_database.h"
#include "handler/crash_report_upload_thread.h"
#include "handler/user_stream_data_source.h"

namespace crashpad {

//! \brief XXX
class CrashReportExceptionHandler {
 public:
  //! \brief XXX
  CrashReportExceptionHandler(
      CrashReportDatabase* database,
      CrashReportUploadThread* upload_thread,
      const std::map<std::string, std::string>* process_annotations,
      const UserStreamDataSources* user_stream_data_sources);

  ~CrashReportExceptionHandler();

 private:
  DISALLOW_COPY_AND_ASSIGN(CrashReportExceptionHandler);
};

}  // namespace crashpad

#endif  // CRASHPAD_HANDLER_FUCHSIA_CRASH_REPORT_EXCEPTION_HANDLER_H_
