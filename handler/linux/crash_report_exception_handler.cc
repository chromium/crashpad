// Copyright 2018 The Crashpad Authors. All rights reserved.
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

#include <memory>
#include <utility>

#include "base/logging.h"
#include "client/settings.h"
#include "handler/linux/capture_snapshot.h"
#include "minidump/minidump_file_writer.h"
#include "snapshot/linux/process_snapshot_linux.h"
#include "snapshot/sanitized/process_snapshot_sanitized.h"
#include "util/file/string_file.h"
#include "util/linux/direct_ptrace_connection.h"
#include "util/linux/ptrace_client.h"
#include "util/misc/implicit_cast.h"
#include "util/misc/metrics.h"
#include "util/misc/uuid.h"
#include "util/stream/base94_output_stream.h"
#include "util/stream/log_output_stream.h"
#include "util/stream/zlib_output_stream.h"

namespace crashpad {

CrashReportExceptionHandler::CrashReportExceptionHandler(
    CrashReportDatabase* database,
    CrashReportUploadThread* upload_thread,
    const std::map<std::string, std::string>* process_annotations,
    Mode mode,
    const UserStreamDataSources* user_stream_data_sources)
    : database_(database),
      upload_thread_(upload_thread),
      process_annotations_(process_annotations),
      mode_(mode),
      user_stream_data_sources_(user_stream_data_sources) {}

CrashReportExceptionHandler::~CrashReportExceptionHandler() = default;

bool CrashReportExceptionHandler::HandleException(
    pid_t client_process_id,
    uid_t client_uid,
    const ExceptionHandlerProtocol::ClientInformation& info,
    VMAddress requesting_thread_stack_address,
    pid_t* requesting_thread_id,
    UUID* local_report_id) {
  Metrics::ExceptionEncountered();

  DirectPtraceConnection connection;
  if (!connection.Initialize(client_process_id)) {
    Metrics::ExceptionCaptureResult(
        Metrics::CaptureResult::kDirectPtraceFailed);
    return false;
  }

  return HandleExceptionWithConnection(&connection,
                                       info,
                                       client_uid,
                                       requesting_thread_stack_address,
                                       requesting_thread_id,
                                       local_report_id);
}

bool CrashReportExceptionHandler::HandleExceptionWithBroker(
    pid_t client_process_id,
    uid_t client_uid,
    const ExceptionHandlerProtocol::ClientInformation& info,
    int broker_sock,
    UUID* local_report_id) {
  Metrics::ExceptionEncountered();

  PtraceClient client;
  if (!client.Initialize(broker_sock, client_process_id)) {
    Metrics::ExceptionCaptureResult(
        Metrics::CaptureResult::kBrokeredPtraceFailed);
    return false;
  }

  return HandleExceptionWithConnection(
      &client, info, client_uid, 0, nullptr, local_report_id);
}

bool CrashReportExceptionHandler::HandleExceptionWithConnection(
    PtraceConnection* connection,
    const ExceptionHandlerProtocol::ClientInformation& info,
    uid_t client_uid,
    VMAddress requesting_thread_stack_address,
    pid_t* requesting_thread_id,
    UUID* local_report_id) {
  std::unique_ptr<ProcessSnapshotLinux> process_snapshot;
  std::unique_ptr<ProcessSnapshotSanitized> sanitized_snapshot;
  if (!CaptureSnapshot(connection,
                       info,
                       *process_annotations_,
                       client_uid,
                       requesting_thread_stack_address,
                       requesting_thread_id,
                       &process_snapshot,
                       &sanitized_snapshot)) {
    return false;
  }

  UUID client_id;
  Settings* const settings = database_->GetSettings();
  if (settings) {
    // If GetSettings() or GetClientID() fails, something else will log a
    // message and client_id will be left at its default value, all zeroes,
    // which is appropriate.
    settings->GetClientID(&client_id);
  }
  process_snapshot->SetClientID(client_id);

  std::unique_ptr<CrashReportDatabase::NewReport> new_report;
  CrashReportDatabase::OperationStatus database_status =
      database_->PrepareNewCrashReport(&new_report);
  if (database_status != CrashReportDatabase::kNoError) {
    LOG(ERROR) << "PrepareNewCrashReport failed";
    Metrics::ExceptionCaptureResult(
        Metrics::CaptureResult::kPrepareNewCrashReportFailed);
    return false;
  }

  process_snapshot->SetReportID(new_report->ReportID());

  ProcessSnapshot* snapshot =
      sanitized_snapshot
          ? implicit_cast<ProcessSnapshot*>(sanitized_snapshot.get())
          : implicit_cast<ProcessSnapshot*>(process_snapshot.get());

  if (mode_ == Mode::kDumpMinidump || mode_ == Mode::kDumpAndLogMinidump) {
    if (!DumpMinidump(snapshot, std::move(new_report), local_report_id))
      return false;
    Metrics::ExceptionCaptureResult(Metrics::CaptureResult::kSuccess);
  }

  if (mode_ == Mode::kLogMinidump || mode_ == Mode::kDumpAndLogMinidump) {
    if (!LogMinidump(snapshot))
      return false;
  }

  return true;
}

bool CrashReportExceptionHandler::DumpMinidump(
    ProcessSnapshot* snapshot,
    std::unique_ptr<CrashReportDatabase::NewReport> new_report,
    UUID* local_report_id) {
  MinidumpFileWriter minidump;
  minidump.InitializeFromSnapshot(snapshot);
  AddUserExtensionStreams(user_stream_data_sources_, snapshot, &minidump);

  if (!minidump.WriteEverything(new_report->Writer())) {
    LOG(ERROR) << "WriteEverything failed";
    Metrics::ExceptionCaptureResult(
        Metrics::CaptureResult::kMinidumpWriteFailed);
    return false;
  }

  UUID uuid;
  auto database_status =
      database_->FinishedWritingCrashReport(std::move(new_report), &uuid);
  if (database_status != CrashReportDatabase::kNoError) {
    LOG(ERROR) << "FinishedWritingCrashReport failed";
    Metrics::ExceptionCaptureResult(
        Metrics::CaptureResult::kFinishedWritingCrashReportFailed);
    return false;
  }

  if (upload_thread_) {
    upload_thread_->ReportPending(uuid);
  }

  if (local_report_id != nullptr) {
    *local_report_id = uuid;
  }
  return true;
}

bool CrashReportExceptionHandler::LogMinidump(ProcessSnapshot* snapshot) {
  MinidumpFileWriter minidump;
  minidump.InitializeFromSnapshot(snapshot);
  AddUserExtensionStreams(user_stream_data_sources_, snapshot, &minidump);

  StringFile minidump_string_file;
  minidump.WriteEverything(&minidump_string_file);
  ZlibOutputStream stream(ZlibOutputStream::Mode::kCompress,
                          std::make_unique<Base94OutputStream>(
                              Base94OutputStream::Mode::kEncode,
                              std::make_unique<LogOutputStream>()));

  if (!stream.Write(reinterpret_cast<const uint8_t*>(
                        minidump_string_file.string().data()),
                    minidump_string_file.string().size()))
    return false;
  return stream.Flush();
}

}  // namespace crashpad
