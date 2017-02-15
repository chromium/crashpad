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

#include "handler/crash_report_upload_thread.h"

#include <errno.h>
#include <time.h>

#include <map>
#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "client/settings.h"
#include "snapshot/minidump/process_snapshot_minidump.h"
#include "snapshot/module_snapshot.h"
#include "util/file/file_reader.h"
#include "util/misc/metrics.h"
#include "util/misc/uuid.h"
#include "util/net/http_body.h"
#include "util/net/http_multipart_builder.h"
#include "util/net/http_transport.h"
#include "util/stdlib/map_insert.h"

namespace crashpad {

namespace {

void InsertOrReplaceMapEntry(std::map<std::string, std::string>* map,
                             const std::string& key,
                             const std::string& value) {
  std::string old_value;
  if (!MapInsertOrReplace(map, key, value, &old_value)) {
    LOG(WARNING) << "duplicate key " << key << ", discarding value "
                 << old_value;
  }
}

// Given a minidump file readable by |minidump_file_reader|, returns a map of
// key-value pairs to use as HTTP form parameters for upload to a Breakpad
// server. The map is built by combining the process simple annotations map with
// each module’s simple annotations map. In the case of duplicate keys, the map
// will retain the first value found for any key, and will log a warning about
// discarded values. Each module’s annotations vector is also examined and built
// into a single string value, with distinct elements separated by newlines, and
// stored at the key named “list_annotations”, which supersedes any other key
// found by that name. The client ID stored in the minidump is converted to
// a string and stored at the key named “guid”, which supersedes any other key
// found by that name.
//
// In the event of an error reading the minidump file, a message will be logged.
std::map<std::string, std::string> BreakpadHTTPFormParametersFromMinidump(
    FileReader* minidump_file_reader) {
  ProcessSnapshotMinidump minidump_process_snapshot;
  if (!minidump_process_snapshot.Initialize(minidump_file_reader)) {
    return std::map<std::string, std::string>();
  }

  std::map<std::string, std::string> parameters =
      minidump_process_snapshot.AnnotationsSimpleMap();

  std::string list_annotations;
  for (const ModuleSnapshot* module : minidump_process_snapshot.Modules()) {
    for (const auto& kv : module->AnnotationsSimpleMap()) {
      if (!parameters.insert(kv).second) {
        LOG(WARNING) << "duplicate key " << kv.first << ", discarding value "
                     << kv.second;
      }
    }

    for (std::string annotation : module->AnnotationsVector()) {
      list_annotations.append(annotation);
      list_annotations.append("\n");
    }
  }

  if (!list_annotations.empty()) {
    // Remove the final newline character.
    list_annotations.resize(list_annotations.size() - 1);

    InsertOrReplaceMapEntry(&parameters, "list_annotations", list_annotations);
  }

  UUID client_id;
  minidump_process_snapshot.ClientID(&client_id);
  InsertOrReplaceMapEntry(&parameters, "guid", client_id.ToString());

  return parameters;
}

// Calls CrashReportDatabase::RecordUploadAttempt() with |successful| set to
// false upon destruction unless disarmed by calling Fire() or Disarm(). Fire()
// triggers an immediate call. Armed upon construction.
class CallRecordUploadAttempt {
 public:
  CallRecordUploadAttempt(CrashReportDatabase* database,
                          const CrashReportDatabase::Report* report)
      : database_(database),
        report_(report) {
  }

  ~CallRecordUploadAttempt() {
    Fire();
  }

  void Fire() {
    if (report_) {
      database_->RecordUploadAttempt(report_, false, std::string());
    }

    Disarm();
  }

  void Disarm() {
    report_ = nullptr;
  }

 private:
  CrashReportDatabase* database_;  // weak
  const CrashReportDatabase::Report* report_;  // weak

  DISALLOW_COPY_AND_ASSIGN(CallRecordUploadAttempt);
};

}  // namespace

CrashReportUploadThread::CrashReportUploadThread(CrashReportDatabase* database,
                                                 const std::string& url,
                                                 bool rate_limit,
                                                 bool upload_gzip)
    : url_(url),
      // Check for pending reports every 15 minutes, even in the absence of a
      // signal from the handler thread. This allows for failed uploads to be
      // retried periodically, and for pending reports written by other
      // processes to be recognized.
      thread_(15 * 60, this),
      database_(database),
      rate_limit_(rate_limit),
      upload_gzip_(upload_gzip) {
}

CrashReportUploadThread::~CrashReportUploadThread() {
}

void CrashReportUploadThread::Start() {
  thread_.Start(0);
}

void CrashReportUploadThread::Stop() {
  thread_.Stop();
}

void CrashReportUploadThread::ReportPending() {
  thread_.DoWorkNow();
}

void CrashReportUploadThread::ProcessPendingReports() {
  std::vector<CrashReportDatabase::Report> reports;
  if (database_->GetPendingReports(&reports) != CrashReportDatabase::kNoError) {
    // The database is sick. It might be prudent to stop trying to poke it from
    // this thread by abandoning the thread altogether. On the other hand, if
    // the problem is transient, it might be possible to talk to it again on the
    // next pass. For now, take the latter approach.
    return;
  }

  for (const CrashReportDatabase::Report& report : reports) {
    ProcessPendingReport(report);

    // Respect Stop() being called after at least one attempt to process a
    // report.
    if (!thread_.is_running()) {
      return;
    }
  }
}

void CrashReportUploadThread::ProcessPendingReport(
    const CrashReportDatabase::Report& report) {
  Settings* const settings = database_->GetSettings();

  bool uploads_enabled;
  if (url_.empty() ||
      (!report.upload_explicitly_requested &&
       (!settings->GetUploadsEnabled(&uploads_enabled) || !uploads_enabled))) {
    // Don’t attempt an upload if there’s no URL to upload to. Allow upload if
    // it has been explicitly requested by the user, otherwise, respect the
    // upload-enabled state stored in the database’s settings.
    database_->SkipReportUpload(report.uuid,
                                Metrics::CrashSkippedReason::kUploadsDisabled);
    return;
  }

  // This currently implements very simplistic rate-limiting, compatible with
  // the Breakpad client, where the strategy is to permit one upload attempt per
  // hour, and retire reports that would exceed this limit or for which the
  // upload fails on the first attempt.
  //
  // If upload was requested explicitly (i.e. by user action), we do not
  // throttle the upload.
  //
  // TODO(mark): Provide a proper rate-limiting strategy and allow for failed
  // upload attempts to be retried.
  if (!report.upload_explicitly_requested && rate_limit_) {
    time_t last_upload_attempt_time;
    if (settings->GetLastUploadAttemptTime(&last_upload_attempt_time)) {
      time_t now = time(nullptr);
      if (now >= last_upload_attempt_time) {
        // If the most recent upload attempt occurred within the past hour,
        // don’t attempt to upload the new report. If it happened longer ago,
        // attempt to upload the report.
        const int kUploadAttemptIntervalSeconds = 60 * 60;  // 1 hour
        if (now - last_upload_attempt_time < kUploadAttemptIntervalSeconds) {
          database_->SkipReportUpload(
              report.uuid, Metrics::CrashSkippedReason::kUploadThrottled);
          return;
        }
      } else {
        // The most recent upload attempt purportedly occurred in the future. If
        // it “happened” at least one day in the future, assume that the last
        // upload attempt time is bogus, and attempt to upload the report. If
        // the most recent upload time is in the future but within one day,
        // accept it and don’t attempt to upload the report.
        const int kBackwardsClockTolerance = 60 * 60 * 24;  // 1 day
        if (last_upload_attempt_time - now < kBackwardsClockTolerance) {
          database_->SkipReportUpload(
              report.uuid, Metrics::CrashSkippedReason::kUnexpectedTime);
          return;
        }
      }
    }
  }

  const CrashReportDatabase::Report* upload_report;
  CrashReportDatabase::OperationStatus status =
      database_->GetReportForUploading(report.uuid, &upload_report);
  switch (status) {
    case CrashReportDatabase::kNoError:
      break;

    case CrashReportDatabase::kBusyError:
      return;

    case CrashReportDatabase::kReportNotFound:
    case CrashReportDatabase::kFileSystemError:
    case CrashReportDatabase::kDatabaseError:
      // In these cases, SkipReportUpload() might not work either, but it’s best
      // to at least try to get the report out of the way.
      database_->SkipReportUpload(report.uuid,
                                  Metrics::CrashSkippedReason::kDatabaseError);
      return;

    case CrashReportDatabase::kCannotRequestUpload:
      NOTREACHED();
      return;
  }

  CallRecordUploadAttempt call_record_upload_attempt(database_, upload_report);

  std::string response_body;
  UploadResult upload_result = UploadReport(upload_report, &response_body);
  switch (upload_result) {
    case UploadResult::kSuccess:
      call_record_upload_attempt.Disarm();
      database_->RecordUploadAttempt(upload_report, true, response_body);
      break;
    case UploadResult::kPermanentFailure:
    case UploadResult::kRetry:
      call_record_upload_attempt.Fire();

      // TODO(mark): Deal with retries properly: don’t call SkipReportUplaod()
      // if the result was kRetry and the report hasn’t already been retried
      // too many times.
      database_->SkipReportUpload(report.uuid,
                                  Metrics::CrashSkippedReason::kUploadFailed);
      break;
  }
}

CrashReportUploadThread::UploadResult CrashReportUploadThread::UploadReport(
    const CrashReportDatabase::Report* report,
    std::string* response_body) {
  std::map<std::string, std::string> parameters;

  {
    FileReader minidump_file_reader;
    if (!minidump_file_reader.Open(report->file_path)) {
      // If the minidump file can’t be opened, all hope is lost.
      return UploadResult::kPermanentFailure;
    }

    // If the minidump file could be opened, ignore any errors that might occur
    // when attempting to interpret it. This may result in its being uploaded
    // with few or no parameters, but as long as there’s a dump file, the server
    // can decide what to do with it.
    parameters = BreakpadHTTPFormParametersFromMinidump(&minidump_file_reader);
  }

  HTTPMultipartBuilder http_multipart_builder;
  http_multipart_builder.SetGzipEnabled(upload_gzip_);

  const char kMinidumpKey[] = "upload_file_minidump";

  for (const auto& kv : parameters) {
    if (kv.first == kMinidumpKey) {
      LOG(WARNING) << "reserved key " << kv.first << ", discarding value "
                   << kv.second;
    } else {
      http_multipart_builder.SetFormData(kv.first, kv.second);
    }
  }

  http_multipart_builder.SetFileAttachment(
      kMinidumpKey,
#if defined(OS_WIN)
      base::UTF16ToUTF8(report->file_path.BaseName().value()),
#else
      report->file_path.BaseName().value(),
#endif
      report->file_path,
      "application/octet-stream");

  std::unique_ptr<HTTPTransport> http_transport(HTTPTransport::Create());
  http_transport->SetURL(url_);
  HTTPHeaders content_headers;
  http_multipart_builder.PopulateContentHeaders(&content_headers);
  for (const auto& content_header : content_headers) {
    http_transport->SetHeader(content_header.first, content_header.second);
  }
  http_transport->SetBodyStream(http_multipart_builder.GetBodyStream());
  // TODO(mark): The timeout should be configurable by the client.
  http_transport->SetTimeout(60.0);  // 1 minute.

  if (!http_transport->ExecuteSynchronously(response_body)) {
    return UploadResult::kRetry;
  }

  return UploadResult::kSuccess;
}

void CrashReportUploadThread::DoWork(const WorkerThread* thread) {
  ProcessPendingReports();
}

}  // namespace crashpad
