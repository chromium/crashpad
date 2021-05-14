// Copyright 2020 The Crashpad Authors. All rights reserved.
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

#include "client/ios_handler/in_process_handler.h"

#include <stdio.h>
#include <sys/stat.h>

#include "base/logging.h"
#include "client/ios_handler/in_process_intermediatedump_handler.h"
#include "client/settings.h"
#include "minidump/minidump_file_writer.h"
#include "util/file/directory_reader.h"
#include "util/file/filesystem.h"

namespace {

// Creates directory at |path|.
void CreateDirectory(const base::FilePath& path) {
  if (mkdir(path.value().c_str(), 0755) == 0) {
    return;
  }
  if (errno != EEXIST) {
    PLOG(ERROR) << "mkdir " << path.value();
  }
}

}  // namespace

namespace crashpad {
namespace internal {

InProcessHandler::InProcessHandler() = default;

InProcessHandler::~InProcessHandler() {
  upload_thread_->Stop();
}

bool InProcessHandler::Initialize(
    const base::FilePath& database,
    const std::string& url,
    const std::map<std::string, std::string>& annotations) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);
  annotations_ = annotations;
  database_ = CrashReportDatabase::Initialize(database);

  if (!url.empty()) {
    // TODO(scottmg): options.rate_limit should be removed when we have a
    // configurable database setting to control upload limiting.
    // See https://crashpad.chromium.org/bug/23.
    CrashReportUploadThread::Options upload_thread_options;
    upload_thread_options.rate_limit = false;
    upload_thread_options.upload_gzip = true;
    upload_thread_options.watch_pending_reports = true;
    upload_thread_options.identify_client_via_url = true;

    upload_thread_.reset(new CrashReportUploadThread(
        database_.get(), url, upload_thread_options));
  }

  // Get the real path somehow.
  CreateDirectory(database);
  static constexpr char kPendingSerializediOSDump[] =
      "pending-serialized-ios-dump";
  base_dir_ = database.Append(kPendingSerializediOSDump);
  CreateDirectory(base_dir_);

  if (!OpenNewFile())
    return false;

  // TODO: Is it safe to call this here?  Is it too soon to use metrics?
  Metrics::HandlerLifetimeMilestone(Metrics::LifetimeMilestone::kStarted);

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

void InProcessHandler::DumpExceptionFromSignal(
    const IOSSystemDataCollector& system_data,
    siginfo_t* siginfo,
    ucontext_t* context) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  ScopedReport report(this, system_data);
  InProcessIntermediatedumpHandler::WriteExceptionFromSignal(
      writer_.get(), system_data, siginfo, context);
}

void InProcessHandler::DumpExceptionFromMachException(
    const IOSSystemDataCollector& system_data,
    exception_behavior_t behavior,
    thread_t thread,
    exception_type_t exception,
    const mach_exception_data_type_t* code,
    mach_msg_type_number_t code_count,
    thread_state_flavor_t flavor,
    ConstThreadState old_state,
    mach_msg_type_number_t old_state_count) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  ScopedReport report(this, system_data);
  InProcessIntermediatedumpHandler::WriteMachExceptionInfo(writer_.get(),
                                                           behavior,
                                                           thread,
                                                           exception,
                                                           code,
                                                           code_count,
                                                           flavor,
                                                           old_state,
                                                           old_state_count);
}

void InProcessHandler::DumpExceptionFromNSExceptionFrames(
    const IOSSystemDataCollector& system_data,
    const uint64_t* frames,
    const size_t num_frames) {
  ScopedReport report(this, system_data, frames, num_frames);
  InProcessIntermediatedumpHandler::WriteNSException(writer_.get());
}

void InProcessHandler::ProcessIntermediateDumps(
    const std::map<std::string, std::string>& extra_annotations) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  std::map<std::string, std::string> annotations;
  annotations.insert(annotations_.begin(), annotations_.end());
  annotations.insert(extra_annotations.begin(), extra_annotations.end());

  std::vector<base::FilePath> files = PendingFiles();
  for (auto& file : files) {
    ProcessSnapshotIOSIntermediateDump process_snapshot;
    if (process_snapshot.Initialize(file, annotations)) {
      SaveSnapshot(process_snapshot);
    }
    LoggingRemoveFile(file);
  }
}

void InProcessHandler::ProcessIntermediateDump(
    const base::FilePath& file,
    const std::map<std::string, std::string>& extra_annotations) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  std::map<std::string, std::string> annotations;
  annotations.insert(annotations_.begin(), annotations_.end());
  annotations.insert(extra_annotations.begin(), extra_annotations.end());

  ProcessSnapshotIOSIntermediateDump process_snapshot;
  if (process_snapshot.Initialize(file, annotations)) {
    SaveSnapshot(process_snapshot);
  }
  LoggingRemoveFile(file);
}

void InProcessHandler::StartProcesingPendingReports() {
  if (!upload_thread_started_ && upload_thread_) {
    upload_thread_->Start();
    upload_thread_started_ = true;
  }
}

void InProcessHandler::SaveSnapshot(
    ProcessSnapshotIOSIntermediateDump& process_snapshot) {
  std::unique_ptr<CrashReportDatabase::NewReport> new_report;
  CrashReportDatabase::OperationStatus database_status =
      database_->PrepareNewCrashReport(&new_report);
  if (database_status != CrashReportDatabase::kNoError) {
    Metrics::ExceptionCaptureResult(
        Metrics::CaptureResult::kPrepareNewCrashReportFailed);
  }
  process_snapshot.SetReportID(new_report->ReportID());

  MinidumpFileWriter minidump;
  minidump.InitializeFromSnapshot(&process_snapshot);
  if (!minidump.WriteEverything(new_report->Writer())) {
    Metrics::ExceptionCaptureResult(
        Metrics::CaptureResult::kMinidumpWriteFailed);
  }
  UUID uuid;
  database_status =
      database_->FinishedWritingCrashReport(std::move(new_report), &uuid);
  if (database_status != CrashReportDatabase::kNoError) {
    Metrics::ExceptionCaptureResult(
        Metrics::CaptureResult::kFinishedWritingCrashReportFailed);
  }

  if (upload_thread_) {
    upload_thread_->ReportPending(uuid);
  }
}

bool InProcessHandler::OpenNewFile() {
  DCHECK(!writer_);
  UUID uuid;
  uuid.InitializeWithNew();
  const std::string uuid_string = uuid.ToString();
  current_file_ = base_dir_.Append(uuid_string);
  writer_ = std::make_unique<IOSIntermediateDumpWriter>();
  if (!writer_->Open(current_file_)) {
    DLOG(ERROR) << "Unable to open intermediate dump file.";
    return false;
  }
  return true;
}

std::vector<base::FilePath> InProcessHandler::PendingFiles() {
  DirectoryReader reader;
  std::vector<base::FilePath> files;
  if (!reader.Open(base_dir_)) {
    return files;
  }
  base::FilePath file;
  DirectoryReader::Result result;
  while ((result = reader.NextFile(&file)) ==
         DirectoryReader::Result::kSuccess) {
    file = base_dir_.Append(file);
    if (file != current_file_) {
      ScopedFileHandle fd(LoggingOpenFileForRead(file));
      if (LoggingLockFile(fd.get(),
                          FileLocking::kExclusive,
                          FileLockingBlocking::kNonBlocking) ==
          FileLockingResult::kSuccess) {
        files.push_back(file);
      }
    }
  }
  return files;
}

InProcessHandler::ScopedReport::ScopedReport(
    InProcessHandler* handler,
    const IOSSystemDataCollector& system_data,
    const uint64_t* frames,
    const size_t num_frames) {
  handler_ = handler;
  InProcessIntermediatedumpHandler::WriteHeader(handler_->writer_.get());
  InProcessIntermediatedumpHandler::WriteProcessInfo(handler_->writer_.get());
  InProcessIntermediatedumpHandler::WriteSystemInfo(handler_->writer_.get(),
                                                    system_data);
  InProcessIntermediatedumpHandler::WriteThreadInfo(
      handler_->writer_.get(), frames, num_frames);
  InProcessIntermediatedumpHandler::WriteModuleInfo(handler_->writer_.get());
}

InProcessHandler::ScopedReport::~ScopedReport() {
  handler_->writer_->Close();
  handler_->writer_.reset();
  // Immediately open a new file for subsequent reports.
  handler_->OpenNewFile();
}

}  // namespace internal
}  // namespace crashpad
