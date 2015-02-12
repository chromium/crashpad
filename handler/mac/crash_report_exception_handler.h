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

#ifndef CRASHPAD_HANDLER_MAC_CRASH_REPORT_EXCEPTION_HANDLER_H_
#define CRASHPAD_HANDLER_MAC_CRASH_REPORT_EXCEPTION_HANDLER_H_

#include <mach/mach.h>

#include "base/basictypes.h"
#include "client/crash_report_database.h"
#include "handler/mac/crash_report_upload_thread.h"
#include "util/mach/exc_server_variants.h"

namespace crashpad {

//! \brief An exception handler that writes crash reports for exception messages
//!     to a CrashReportDatabase.
class CrashReportExceptionHandler : public UniversalMachExcServer::Interface {
 public:
  //! \brief Creates a new object that will store crash reports in \a database.
  //!
  //! \param[in] database The database to store crash reports in. Weak.
  //! \param[in] upload_thread The upload thread to notify when a new crash
  //!     report is written into \a database.
  CrashReportExceptionHandler(CrashReportDatabase* database,
                              CrashReportUploadThread* upload_thread);

  ~CrashReportExceptionHandler();

  // UniversalMachExcServer::Interface:

  //! \brief Processes an exception message by writing a crash report to this
  //!     object’s CrashReportDatabase.
  kern_return_t CatchMachException(
      exception_behavior_t behavior,
      exception_handler_t exception_port,
      thread_t thread,
      task_t task,
      exception_type_t exception,
      const mach_exception_data_type_t* code,
      mach_msg_type_number_t code_count,
      thread_state_flavor_t* flavor,
      const natural_t* old_state,
      mach_msg_type_number_t old_state_count,
      thread_state_t new_state,
      mach_msg_type_number_t* new_state_count,
      const mach_msg_trailer_t* trailer,
      bool* destroy_complex_request) override;

 private:
  CrashReportDatabase* database_;  // weak
  CrashReportUploadThread* upload_thread_;  // weak

  DISALLOW_COPY_AND_ASSIGN(CrashReportExceptionHandler);
};

}  // namespace crashpad

#endif  // CRASHPAD_HANDLER_MAC_CRASH_REPORT_EXCEPTION_HANDLER_H_
