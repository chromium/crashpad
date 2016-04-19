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

#include "handler/win/crash_report_exception_handler.h"

#include "client/crash_report_database.h"
#include "client/settings.h"
#include "handler/crash_report_upload_thread.h"
#include "minidump/minidump_file_writer.h"
#include "snapshot/win/process_snapshot_win.h"
#include "util/file/file_writer.h"
#include "util/win/pe_image_reader.h"
#include "util/win/registration_protocol_win.h"
#include "util/win/scoped_process_suspend.h"

namespace crashpad {

CrashReportExceptionHandler::CrashReportExceptionHandler(
    CrashReportDatabase* database,
    CrashReportUploadThread* upload_thread,
    const std::map<std::string, std::string>* process_annotations)
    : database_(database),
      upload_thread_(upload_thread),
      process_annotations_(process_annotations) {
}

CrashReportExceptionHandler::~CrashReportExceptionHandler() {
}

void CrashReportExceptionHandler::ExceptionHandlerServerStarted() {
}

unsigned int CrashReportExceptionHandler::ExceptionHandlerServerException(
    HANDLE process,
    WinVMAddress exception_information_address,
    WinVMAddress debug_critical_section_address) {
  const unsigned int kFailedTerminationCode = 0xffff7002;

  ScopedProcessSuspend suspend(process);

  ProcessSnapshotWin process_snapshot;
  if (!process_snapshot.Initialize(process,
                                   ProcessSuspensionState::kSuspended,
                                   debug_critical_section_address)) {
    LOG(WARNING) << "ProcessSnapshotWin::Initialize failed";
    return kFailedTerminationCode;
  }

  // Check if the target has a CrashpadInfo structure, and if it does, if there
  // was a thread ID and exception code stashed into the target's CrashpadInfo.
  // This is done by CrashpadClient::DumpAndCrashTargetProcess() to indicate the
  // thread that should be blamed in a fabricated exception. If the thread id is
  // 0 or we are unable to retrieve a CrashpadInfo, process normally.
  auto main_module = static_cast<const internal::ModuleSnapshotWin*>(
      process_snapshot.Modules().front());
  const PEImageReader& main_module_pe_reader = main_module->pe_image_reader();
  DWORD target_thread_id = 0;
  DWORD target_exception_code = 0;
  if (process_snapshot.process_reader().Is64Bit()) {
    process_types::CrashpadInfo<process_types::internal::Traits64> info;
    if (main_module_pe_reader.GetCrashpadInfo(&info, nullptr)) {
      target_thread_id = info.target_thread_id;
      target_exception_code = info.target_exception_code;
    }
  } else {
    process_types::CrashpadInfo<process_types::internal::Traits32> info;
    if (main_module_pe_reader.GetCrashpadInfo(&info, nullptr)) {
      target_thread_id = info.target_thread_id;
      target_exception_code = info.target_exception_code;
    }
  }

  if (target_thread_id != 0) {
    // Fabricate a fake exception in the given thread with exception code that
    // the caller supplied.
    process_snapshot.InitializeWithFabricatedException(target_thread_id,
                                                       target_exception_code);

  } else {
    if (!process_snapshot.InitializeException(exception_information_address)) {
      LOG(WARNING) << "ProcessSnapshotWin::InitializeException failed";
      return kFailedTerminationCode;
    }
  }

  // Now that we have the exception information, even if something else fails we
  // can terminate the process with the correct exit code.
  const unsigned int termination_code =
      process_snapshot.Exception()->Exception();

  CrashpadInfoClientOptions client_options;
  process_snapshot.GetCrashpadOptions(&client_options);
  if (client_options.crashpad_handler_behavior != TriState::kDisabled) {
    UUID client_id;
    Settings* const settings = database_->GetSettings();
    if (settings) {
      // If GetSettings() or GetClientID() fails, something else will log a
      // message and client_id will be left at its default value, all zeroes,
      // which is appropriate.
      settings->GetClientID(&client_id);
    }

    process_snapshot.SetClientID(client_id);
    process_snapshot.SetAnnotationsSimpleMap(*process_annotations_);

    CrashReportDatabase::NewReport* new_report;
    CrashReportDatabase::OperationStatus database_status =
        database_->PrepareNewCrashReport(&new_report);
    if (database_status != CrashReportDatabase::kNoError) {
      LOG(ERROR) << "PrepareNewCrashReport failed";
      return termination_code;
    }

    process_snapshot.SetReportID(new_report->uuid);

    CrashReportDatabase::CallErrorWritingCrashReport
        call_error_writing_crash_report(database_, new_report);

    WeakFileHandleFileWriter file_writer(new_report->handle);

    MinidumpFileWriter minidump;
    minidump.InitializeFromSnapshot(&process_snapshot);
    if (!minidump.WriteEverything(&file_writer)) {
      LOG(ERROR) << "WriteEverything failed";
      return termination_code;
    }

    call_error_writing_crash_report.Disarm();

    UUID uuid;
    database_status = database_->FinishedWritingCrashReport(new_report, &uuid);
    if (database_status != CrashReportDatabase::kNoError) {
      LOG(ERROR) << "FinishedWritingCrashReport failed";
      return termination_code;
    }

    upload_thread_->ReportPending();
  }

  return termination_code;
}

}  // namespace crashpad
