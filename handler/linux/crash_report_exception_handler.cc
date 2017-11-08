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

#include "handler/linux/crash_report_exception_handler.h"

#include <vector>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "client/settings.h"
#include "minidump/minidump_file_writer.h"
#include "minidump/minidump_user_extension_stream_data_source.h"
#include "snapshot/crashpad_info_client_options.h"
#include "snapshot/linux/process_snapshot_linux.h"
#include "util/file/file_writer.h"
#include "util/linux/direct_ptrace_connection.h"
#include "util/misc/metrics.h"
#include "util/misc/tri_state.h"
#include "util/misc/uuid.h"

namespace crashpad {

CrashReportExceptionHandler::CrashReportExceptionHandler(
    CrashReportDatabase* database,
    CrashReportUploadThread* upload_thread,
    const std::map<std::string, std::string>* process_annotations,
    const UserStreamDataSources* user_stream_data_sources)
    : database_(database),
      upload_thread_(upload_thread),
      process_annotations_(process_annotations),
      user_stream_data_sources_(user_stream_data_sources) {}

CrashReportExceptionHandler::~CrashReportExceptionHandler() {
}

bool CrashReportExceptionHandler::HandleException(
    pid_t client_process_id,
    LinuxVMAddress exception_information_address) {
  Metrics::ExceptionEncountered();

  // TODO(jperaza): support a brokered connection
  DirectPtraceConnection connection;
  if (!connection.Initialize(client_process_id)) {
    return false;
  }

  ProcessSnapshotLinux process_snapshot;
  if (!process_snapshot.Initialize(&connection)) {
    LOG(WARNING) << "ProcessSnapshotLinux::Initialize failed";
    Metrics::ExceptionCaptureResult(Metrics::CaptureResult::kSnapshotFailed);
    return false;
  }

  if (!process_snapshot.InitializeException(exception_information_address)) {
    return false;
  }

  Metrics::ExceptionCode(process_snapshot.Exception()->Exception());

  // TODO(jperaza): get client options to honor disabling of the handler
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

  std::unique_ptr<CrashReportDatabase::NewReport> new_report;
  CrashReportDatabase::OperationStatus database_status =
      database_->PrepareNewCrashReport(&new_report);
  if (database_status != CrashReportDatabase::kNoError) {
    LOG(ERROR) << "PrepareNewCrashReport failed";
    Metrics::ExceptionCaptureResult(
        Metrics::CaptureResult::kPrepareNewCrashReportFailed);
    return false;
  }

  process_snapshot.SetReportID(new_report->uuid);

  MinidumpFileWriter minidump;
  minidump.InitializeFromSnapshot(&process_snapshot);
  AddUserExtensionStreams(
      user_stream_data_sources_, &process_snapshot, &minidump);

  if (!minidump.WriteEverything(new_report->Writer())) {
    LOG(ERROR) << "WriteEverything failed";
    Metrics::ExceptionCaptureResult(
        Metrics::CaptureResult::kMinidumpWriteFailed);
    return false;
  }

  UUID uuid;
  database_status = database_->FinishedWritingCrashReport(std::move(new_report), &uuid);
  if (database_status != CrashReportDatabase::kNoError) {
    LOG(ERROR) << "FinishedWritingCrashReport failed";
    Metrics::ExceptionCaptureResult(
        Metrics::CaptureResult::kFinishedWritingCrashReportFailed);
    return false;
  }

  upload_thread_->ReportPending(uuid);

  Metrics::ExceptionCaptureResult(Metrics::CaptureResult::kSuccess);
  return true;
}

}  // namespace crashpad
