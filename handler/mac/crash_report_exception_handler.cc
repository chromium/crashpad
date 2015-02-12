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

#include "handler/mac/crash_report_exception_handler.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "minidump/minidump_file_writer.h"
#include "snapshot/mac/process_snapshot_mac.h"
#include "util/file/file_writer.h"
#include "util/mach/exception_behaviors.h"
#include "util/mach/mach_extensions.h"
#include "util/mach/scoped_task_suspend.h"
#include "util/misc/uuid.h"

namespace crashpad {

namespace {

// Calls CrashReportDatabase::ErrorWritingCrashReport() upon destruction unless
// disarmed by calling Disarm(). Armed upon construction.
class CallErrorWritingCrashReport {
 public:
  CallErrorWritingCrashReport(CrashReportDatabase* database,
                              CrashReportDatabase::NewReport* new_report)
      : database_(database),
        new_report_(new_report) {
  }

  ~CallErrorWritingCrashReport() {
    if (new_report_) {
      database_->ErrorWritingCrashReport(new_report_);
    }
  }

  void Disarm() {
    new_report_ = nullptr;
  }

 private:
  CrashReportDatabase* database_;  // weak
  CrashReportDatabase::NewReport* new_report_;  // weak

  DISALLOW_COPY_AND_ASSIGN(CallErrorWritingCrashReport);
};

}  // namespace

CrashReportExceptionHandler::CrashReportExceptionHandler(
    CrashReportDatabase* database,
    CrashReportUploadThread* upload_thread)
    : database_(database),
      upload_thread_(upload_thread) {
}

CrashReportExceptionHandler::~CrashReportExceptionHandler() {
}

kern_return_t CrashReportExceptionHandler::CatchMachException(
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
    bool* destroy_complex_request) {
  *destroy_complex_request = true;

  // The expected behavior is EXCEPTION_STATE_IDENTITY | MACH_EXCEPTION_CODES,
  // but it’s possible to deal with any exception behavior as long as it
  // carries identity information (valid thread and task ports).
  if (!ExceptionBehaviorHasIdentity(behavior)) {
    LOG(ERROR) << base::StringPrintf(
        "unexpected exception behavior 0x%x, rejecting", behavior);
    return KERN_FAILURE;
  } else if (behavior != (EXCEPTION_STATE_IDENTITY | kMachExceptionCodes)) {
    LOG(WARNING) << base::StringPrintf(
        "unexpected exception behavior 0x%x, proceeding", behavior);
  }

  if (task == mach_task_self()) {
    LOG(ERROR) << "cannot suspend myself";
    return KERN_FAILURE;
  }

  ScopedTaskSuspend suspend(task);

  ProcessSnapshotMac process_snapshot;
  if (!process_snapshot.Initialize(task)) {
    return KERN_FAILURE;
  }

  CrashReportDatabase::NewReport* new_report;
  CrashReportDatabase::OperationStatus database_status =
      database_->PrepareNewCrashReport(&new_report);
  if (database_status != CrashReportDatabase::kNoError) {
    return KERN_FAILURE;
  }

  CallErrorWritingCrashReport call_error_writing_crash_report(database_,
                                                              new_report);

  WeakFileHandleFileWriter file_writer(new_report->handle);

  MinidumpFileWriter minidump;
  minidump.InitializeFromSnapshot(&process_snapshot);
  if (!minidump.WriteEverything(&file_writer)) {
    return KERN_FAILURE;
  }

  call_error_writing_crash_report.Disarm();

  UUID uuid;
  database_status = database_->FinishedWritingCrashReport(new_report, &uuid);
  if (database_status != CrashReportDatabase::kNoError) {
    return KERN_FAILURE;
  }

  upload_thread_->ReportPending();

  return ExcServerSuccessfulReturnValue(behavior, false);
}

}  // namespace crashpad
