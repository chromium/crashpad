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

#include "client/ios_handler/in_process_handler_util.h"
#include "client/settings.h"
#include "minidump/minidump_file_writer.h"
#include "util/file/directory_reader.h"
#include "util/file/filesystem.h"

namespace {

// Ensures that the node at |path| is a directory. If the |path| refers to a
// file, rather than a directory, returns false. Otherwise, returns true,
// indicating that |path| already was a directory.
bool EnsureDirectoryExists(const base::FilePath& path) {
  struct stat st;
  if (stat(path.value().c_str(), &st) != 0) {
    PLOG(ERROR) << "stat " << path.value();
    return false;
  }
  if (!S_ISDIR(st.st_mode)) {
    LOG(ERROR) << "stat " << path.value() << ": not a directory";
    return false;
  }
  return true;
}

// Ensures that the node at |path| is a directory, and creates it if it does
// not exist. If the |path| refers to a file, rather than a directory, or the
// directory could not be created, returns false. Otherwise, returns true,
// indicating that |path| already was or now is a directory.
bool CreateOrEnsureDirectoryExists(const base::FilePath& path) {
  if (mkdir(path.value().c_str(), 0755) == 0) {
    return true;
  }
  if (errno != EEXIST) {
    PLOG(ERROR) << "mkdir " << path.value();
    return false;
  }
  return EnsureDirectoryExists(path);
}

constexpr char kPendingSerializediOSDump[] = "pending-serialized-ios-dump";

}  // namespace

namespace crashpad {

void InProcessHandler::Initialize(
    const base::FilePath& database,
    const std::string& url,
    const std::map<std::string, std::string>& annotations) {
  annotations_ = annotations;
  database_ = CrashReportDatabase::Initialize(database);
  database_->GetSettings()->SetUploadsEnabled(true);

  if (!url.empty()) {
    // TODO(scottmg): options.rate_limit should be removed when we have a
    // configurable database setting to control upload limiting.
    // See https://crashpad.chromium.org/bug/23.
    CrashReportUploadThread::Options upload_thread_options;
    upload_thread_options.rate_limit = true;
    upload_thread_options.upload_gzip = true;
    upload_thread_options.watch_pending_reports = true;
    upload_thread_options.identify_client_via_url = true;

    upload_thread_ = new CrashReportUploadThread(
        database_.get(), url, upload_thread_options);
    upload_thread_->Start();
  }

  // Get the real path somehow.
  CreateOrEnsureDirectoryExists(database);
  base_dir_ = database.Append(kPendingSerializediOSDump);
  CreateOrEnsureDirectoryExists(base_dir_);

  OpenNewFile();
}

void InProcessHandler::ProcessPendingDumps() {
  std::vector<base::FilePath> files = PendingFiles();
  for (auto& file : files) {
    ProcessSnapshotIOS process_snapshot;
    if (process_snapshot.Initialize(file, annotations_)) {
      SaveSnapshot(process_snapshot);
    }
    LoggingRemoveFile(file);
  }
}

void InProcessHandler::SaveSnapshot(ProcessSnapshotIOS& process_snapshot) {
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

void InProcessHandler::OpenNewFile() {
  UUID uuid;
  uuid.InitializeWithNew();
  const std::string uuid_string = uuid.ToString();
  current_file_ = base_dir_.Append(uuid_string);
  fd_ = open_dprotected_np(current_file_.value().c_str(),
                           O_WRONLY | O_CREAT | O_TRUNC,
                           // PROTECTION_CLASS_D
                           4,
                           // <empty>
                           0,
                           //-rw-r--r--
                           0644);
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
      files.push_back(file);
    }
  }
  return files;
}

void InProcessHandler::DumpExceptionFromSignal(
    const IOSSystemDataCollector& system_data,
    siginfo_t* siginfo,
    ucontext_t* context) {
  StartReport(system_data);
  crashpad::internal::WriteExceptionFromSignal(
      fd_, system_data, siginfo, context);
  EndReport();
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
  StartReport(system_data);
  crashpad::internal::WriteMachExceptionInfo(fd_,
                                             behavior,
                                             thread,
                                             exception,
                                             code,
                                             code_count,
                                             flavor,
                                             old_state,
                                             old_state_count);
  EndReport();
}

void InProcessHandler::StartReport(const IOSSystemDataCollector& system_data) {
  crashpad::internal::WriteHeader(fd_);
  crashpad::internal::WriteProcessInfo(fd_);
  crashpad::internal::WriteSystemInfo(fd_, system_data);
  crashpad::internal::WriteThreadInfo(fd_);
  crashpad::internal::WriteModuleInfo(fd_);
}

void InProcessHandler::EndReport() {
  close(fd_);
  // Immediately open a new file for subsequent reports.
  OpenNewFile();
}

}  // namespace crashpad
