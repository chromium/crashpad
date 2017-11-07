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

#include "util/file/directory_reader.h"
#include "util/file/file_io.h"
#include "util/file/filesystem.h"

namespace crashpad {

namespace {

// Reads from the current file position to EOF and returns as a string of bytes.
std::string ReadRestOfFileAsString(FileHandle handle) {
  FileOffset read_from, end, original;
  if ((read_from = LoggingSeekFile(handle, 0, SEEK_CUR)) == -1 ||
      (end = LoggingSeekFile(handle, 0, SEEK_END)) == -1 ||
      (original = LoggingSeekFile(handle, read_from, SEEK_SET)) == -1 ||
      read_from == end) {
    return std::string();
  }
  DCHECK_EQ(read_from, original);
  DCHECK_GT(end, read_from);

  size_t data_length = static_cast<size_t>(end - read_from);
  std::string buffer(data_length, '\0');
  return LoggingReadFileExactly(handle, &buffer[0], data_length)
             ? buffer
             : std::string();
}

base::FilePath ReplaceFinalExtension(
    const base::FilePath& path,
    const base::FilePath::StringType extension) {
  return base::FilePath(path.RemoveFinalExtension().value() + extension);
}

using OperationStatus = CrashReportDatabase::OperationStatus;

constexpr base::FilePath::CharType kSettings[] =
    FILE_PATH_LITERAL("settings.dat");

constexpr base::FilePath::CharType kCrashReportExtension[] =
    FILE_PATH_LITERAL(".dmp");
constexpr base::FilePath::CharType kMetadataExtension[] =
    FILE_PATH_LITERAL(".meta");
constexpr base::FilePath::CharType kLockExtension[] =
    FILE_PATH_LITERAL(".lock");

constexpr base::FilePath::CharType kNewDirectory[] = FILE_PATH_LITERAL("new");
constexpr base::FilePath::CharType kPendingDirectory[] =
    FILE_PATH_LITERAL("pending");
constexpr base::FilePath::CharType kCompletedDirectory[] =
    FILE_PATH_LITERAL("completed");

constexpr const base::FilePath::CharType* kReportDirectories[] = {
    kNewDirectory,
    kPendingDirectory,
    kCompletedDirectory,
};

enum {
  //! \brief Corresponds to uploaded bit of the report state.
  kAttributeUploaded = 1 << 0,

  //! \brief Corresponds to upload_explicity_requested bit of the report state.
  kAttributeUploadExplicitlyRequested = 1 << 1,
};

struct ReportMetadata {
  static constexpr int32_t kVersion = 1;

  int32_t version = kVersion;
  int32_t upload_attempts = 0;
  int64_t last_upload_attempt_time = 0;
  time_t creation_time = 0;
  uint8_t attributes = 0;
};

}  // namespace

CrashReportDatabase::CrashReportDatabase()
    : base_dir_(), settings_(), initialized_() {}

CrashReportDatabase::~CrashReportDatabase() {}

CrashReportDatabase::Report::Report()
    : uuid(),
      id(),
      creation_time(0),
      uploaded(false),
      last_upload_attempt_time(0),
      upload_attempts(0),
      upload_explicitly_requested(false),
      file_path() {}

CrashReportDatabase::Report::~Report() {}

CrashReportDatabase::NewReport::NewReport()
    : writer_(std::make_unique<FileWriter>()), uuid_(), file_remover_() {}

CrashReportDatabase::NewReport::~NewReport() {}

CrashReportDatabase::ScopedLockFile::ScopedLockFile() : lock_file_() {}

CrashReportDatabase::ScopedLockFile::~ScopedLockFile() {}

CrashReportDatabase::ScopedLockFile& CrashReportDatabase::ScopedLockFile::
operator=(ScopedLockFile&& other) {
  lock_file_.reset(other.lock_file_.release());
  return *this;
}

bool CrashReportDatabase::ScopedLockFile::ResetAcquire(
    const base::FilePath& report_path) {
  lock_file_.reset();

  base::FilePath lock_path(report_path.RemoveFinalExtension().value() +
                           kLockExtension);
  ScopedFileHandle lock_fd(LoggingOpenFileForWrite(
      lock_path, FileWriteMode::kCreateOrFail, FilePermissions::kOwnerOnly));
  if (!lock_fd.is_valid()) {
    return false;
  }
  lock_file_.reset(lock_path);

  time_t timestamp = time(nullptr);
  if (!LoggingWriteFile(lock_fd.get(), &timestamp, sizeof(timestamp))) {
    return false;
  }

  return true;
}

// static
bool CrashReportDatabase::ScopedLockFile::IsExpired(
    const base::FilePath& lock_path, time_t lockfile_ttl) {
  ScopedFileHandle lock_fd(LoggingOpenFileForReadAndWrite(
      lock_path, FileWriteMode::kReuseOrFail, FilePermissions::kOwnerOnly));
  if (!lock_fd.is_valid()) {
    return false;
  }

  time_t now = time(nullptr);

  timespec filetime;
  if (FileHandleModificationTime(lock_fd.get(), &filetime, sizeof(filetime)) &&
    filetime.tv_sec < now + lockfile_ttl) {
    return false;
  }

  time_t timestamp;
  if (!LoggingReadFileExactly(lock_fd.get(), &timestamp, sizeof(timestamp))) {
    return false;
  }

  return now >= timestamp + lockfile_ttl;
}

CrashReportDatabase::ReadReport::ReadReport()
    : Report(), reader_(std::make_unique<FileReader>()), lock_file_() {}

CrashReportDatabase::ReadReport::~ReadReport() {}

CrashReportDatabase::UploadReport::UploadReport()
    : Report(),
      reader_(std::make_unique<FileReader>()),
      lock_file_(),
      database_() {}

CrashReportDatabase::UploadReport::~UploadReport() {
  if (database_) {
    database_->RecordUploadAttempt(this, false, std::string());
  }
}

bool CrashReportDatabase::Initialize(const base::FilePath& path,
                                     bool may_create) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);
  base_dir_ = path;

#if defined(OS_WIN)
  CleanOldWindowsData();
#endif

  if (!IsDirectory(base_dir_, true) &&
      !(may_create &&
        LoggingCreateDirectory(base_dir_, FilePermissions::kOwnerOnly, true))) {
    return false;
  }

  for (size_t i = 0; i < arraysize(kReportDirectories); ++i) {
    if (!LoggingCreateDirectory(base_dir_.Append(kReportDirectories[i]),
                                FilePermissions::kOwnerOnly,
                                true)) {
      return false;
    }
  }

  if (!settings_.Initialize(base_dir_.Append(kSettings))) {
    return false;
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

Settings* CrashReportDatabase::GetSettings() {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &settings_;
}

OperationStatus CrashReportDatabase::PrepareNewCrashReport(
    std::unique_ptr<NewReport>* report) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  auto new_report = std::make_unique<NewReport>();
  if (!new_report->uuid_.InitializeWithNew()) {
    return kDatabaseError;
  }

  const base::FilePath path = ReportPath(new_report->uuid_, kNew);

  if (!new_report->writer_->Open(
          path, FileWriteMode::kCreateOrFail, FilePermissions::kOwnerOnly)) {
    return kFileSystemError;
  }
  new_report->file_remover_.reset(path);

  report->reset(new_report.release());
  return kNoError;
}

OperationStatus CrashReportDatabase::FinishedWritingCrashReport(
    std::unique_ptr<NewReport> report,
    UUID* uuid) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  base::FilePath path = ReportPath(report->uuid_, kPending);
  ScopedLockFile lock_file;
  if (!lock_file.ResetAcquire(path)) {
    return kBusyError;
  }

  if (!WriteNewMetadata(ReplaceFinalExtension(path, kMetadataExtension))) {
    return kDatabaseError;
  }

  FileOffset size = report->writer_->Seek(0, SEEK_END);

  report->writer_->Close();
  if (!MoveFileOrDirectory(report->file_remover_.get(), path)) {
    return kFileSystemError;
  }
  // We've moved the report to pending, so it no longer needs to be removed.
  ignore_result(report->file_remover_.release());

  *uuid = report->uuid_;

  Metrics::CrashReportPending(Metrics::PendingReportReason::kNewlyCreated);
  Metrics::CrashReportSize(size);

  report.reset();
  return kNoError;
}

OperationStatus CrashReportDatabase::LookUpCrashReport(const UUID& uuid,
                                                       Report* report) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  ScopedLockFile lock_file;
  base::FilePath path;
  return CheckoutReport(uuid, kSearchable, &path, &lock_file, report);
}

OperationStatus CrashReportDatabase::GetPendingReports(
    std::vector<Report>* reports) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return ReportsInState(kPending, reports);
}

OperationStatus CrashReportDatabase::GetCompletedReports(
    std::vector<Report>* reports) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return ReportsInState(kCompleted, reports);
}

OperationStatus CrashReportDatabase::GetReportForUploading(
    const UUID& uuid,
    std::unique_ptr<const UploadReport>* report) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  auto upload_report = std::make_unique<UploadReport>();

  base::FilePath path;
  OperationStatus os = CheckoutReport(
      uuid, kPending, &path, &upload_report->lock_file_, upload_report.get());
  if (os != kNoError) {
    return os;
  }

  if (!upload_report->reader_->Open(path)) {
    return kFileSystemError;
  }

  upload_report->database_ = this;
  report->reset(upload_report.release());
  return kNoError;
}

OperationStatus CrashReportDatabase::RecordUploadComplete(
    std::unique_ptr<const UploadReport> report,
    const std::string& id) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  OperationStatus os =
      RecordUploadAttempt(const_cast<UploadReport*>(report.get()), true, id);
  return os;
}

OperationStatus CrashReportDatabase::SkipReportUpload(
    const UUID& uuid,
    Metrics::CrashSkippedReason reason) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  Metrics::CrashUploadSkipped(reason);

  base::FilePath path;
  ScopedLockFile lock_file;
  Report report;
  OperationStatus os =
      CheckoutReport(uuid, kPending, &path, &lock_file, &report);
  if (os != kNoError) {
    return os;
  }

  base::FilePath completed_path(ReportPath(uuid, kCompleted));
  ScopedLockFile completed_lock_file;
  if (!completed_lock_file.ResetAcquire(completed_path)) {
    return kBusyError;
  }

  report.upload_explicitly_requested = false;
  if (!WriteMetadata(completed_path, report)) {
    return kDatabaseError;
  }

  if (!MoveFileOrDirectory(path, completed_path)) {
    return kFileSystemError;
  }

  if (!LoggingRemoveFile(ReplaceFinalExtension(path, kMetadataExtension))) {
    return kDatabaseError;
  }

  return kNoError;
}

OperationStatus CrashReportDatabase::GetReportForReading(
    const UUID& uuid,
    std::unique_ptr<const ReadReport>* report) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  auto read_report = std::make_unique<ReadReport>();

  base::FilePath path;
  ScopedLockFile lock_file;
  OperationStatus os =
      CheckoutReport(uuid, kSearchable, &path, &lock_file, read_report.get());
  if (os != kNoError) {
    return os;
  }

  if (!read_report->reader_->Open(path)) {
    return kFileSystemError;
  }

  report->reset(read_report.release());
  return kNoError;
}

OperationStatus CrashReportDatabase::DeleteReport(const UUID& uuid) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  base::FilePath path;
  ScopedLockFile lock_file;
  OperationStatus os =
      LocateAndLockReport(uuid, kSearchable, &path, &lock_file);
  if (os != kNoError) {
    return os;
  }

  if (!LoggingRemoveFile(path)) {
    return kFileSystemError;
  }

  if (!LoggingRemoveFile(ReplaceFinalExtension(path, kMetadataExtension))) {
    return kDatabaseError;
  }

  return kNoError;
}

OperationStatus CrashReportDatabase::RequestUpload(const UUID& uuid) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  base::FilePath path;
  ScopedLockFile lock_file;
  Report report;
  OperationStatus os =
      CheckoutReport(uuid, kSearchable, &path, &lock_file, &report);
  if (os != kNoError) {
    return os;
  }

  if (report.uploaded) {
    return kCannotRequestUpload;
  }

  report.upload_explicitly_requested = true;
  base::FilePath pending_path = ReportPath(uuid, kPending);
  if (!MoveFileOrDirectory(path, pending_path)) {
    return kFileSystemError;
  }

  if (!WriteMetadata(pending_path, report)) {
    return kDatabaseError;
  }

  if (pending_path != path) {
    if (!LoggingRemoveFile(ReplaceFinalExtension(path, kMetadataExtension))) {
      return kDatabaseError;
    }
  }

  Metrics::CrashReportPending(Metrics::PendingReportReason::kUserInitiated);
  return kNoError;
}

base::FilePath CrashReportDatabase::ReportPath(const UUID& uuid,
                                               ReportState state) {
  DCHECK_NE(state, kUninitialized);
  DCHECK_NE(state, kSearchable);

#if defined(OS_POSIX)
  return base_dir_.Append(kReportDirectories[state])
      .Append(uuid.ToString() + kCrashReportExtension);
#elif defined(OS_WIN)
  return base_dir_.Append(kReportDirectories[state])
      .Append(uuid.ToString16() + kCrashReportExtension);
#endif
}

OperationStatus CrashReportDatabase::LocateAndLockReport(
    const UUID& uuid,
    ReportState desired_state,
    base::FilePath* path,
    ScopedLockFile* lock_file) {
  std::vector<ReportState> searchable_states;
  if (desired_state == kSearchable) {
    searchable_states.push_back(kPending);
    searchable_states.push_back(kCompleted);
  } else {
    DCHECK(desired_state == kPending || desired_state == kCompleted);
    searchable_states.push_back(desired_state);
  }

  for (const ReportState state : searchable_states) {
    base::FilePath local_path(ReportPath(uuid, state));
    ScopedLockFile local_lock;
    if (!local_lock.ResetAcquire(local_path)) {
      return kBusyError;
    }

    if (!IsRegularFile(local_path)) {
      continue;
    }

    *path = local_path;
    *lock_file = std::move(local_lock);
    return kNoError;
  }

  return kReportNotFound;
}

OperationStatus CrashReportDatabase::CheckoutReport(const UUID& uuid,
                                                    ReportState state,
                                                    base::FilePath* path,
                                                    ScopedLockFile* lock_file,
                                                    Report* report) {
  ScopedLockFile local_lock;
  base::FilePath local_path;
  OperationStatus os =
      LocateAndLockReport(uuid, state, &local_path, &local_lock);
  if (os != kNoError) {
    return os;
  }

  if (!CleaningReadMetadata(local_path, report)) {
    return kDatabaseError;
  }

  *path = local_path;
  *lock_file = std::move(local_lock);
  return kNoError;
}

OperationStatus CrashReportDatabase::ReportsInState(
    ReportState state,
    std::vector<Report>* reports) {
  DCHECK(reports->empty());
  DCHECK_NE(state, kUninitialized);
  DCHECK_NE(state, kSearchable);
  DCHECK_NE(state, kNew);

  const base::FilePath dir_path(base_dir_.Append(kReportDirectories[state]));
  DirectoryReader reader;
  if (!reader.Open(dir_path)) {
    return CrashReportDatabase::kDatabaseError;
  }

  base::FilePath filename;
  DirectoryReader::Result result;
  while ((result = reader.NextFile(&filename)) ==
         DirectoryReader::Result::kSuccess) {
    const base::FilePath::StringType extension(filename.FinalExtension());
    const base::FilePath filepath(dir_path.Append(filename));

    if (extension.compare(kCrashReportExtension) != 0) {
      continue;
    }

    ScopedLockFile lock_file;
    if (!lock_file.ResetAcquire(filepath)) {
      continue;
    }

    Report report;
    if (!CleaningReadMetadata(filepath, &report)) {
      continue;
    }
    reports->push_back(report);
    reports->back().file_path = filepath;
  }
  return kNoError;
}

OperationStatus CrashReportDatabase::RecordUploadAttempt(
    UploadReport* report,
    bool successful,
    const std::string& id) {
  Metrics::CrashUploadAttempted(successful);
  report->database_ = nullptr;

  time_t now = time(nullptr);

  report->id = id;
  report->uploaded = successful;
  report->last_upload_attempt_time = now;
  ++report->upload_attempts;

  base::FilePath report_path(report->file_path);

  ScopedLockFile lock_file;
  if (successful) {
    report->upload_explicitly_requested = false;

    base::FilePath completed_report_path = ReportPath(report->uuid, kCompleted);

    if (!lock_file.ResetAcquire(completed_report_path)) {
      return kBusyError;
    }

    report->reader_->Close();
    if (!MoveFileOrDirectory(report_path, completed_report_path)) {
      return kFileSystemError;
    }

    LoggingRemoveFile(ReplaceFinalExtension(report_path, kMetadataExtension));
    report_path = completed_report_path;
  }

  if (!WriteMetadata(report_path, *report)) {
    return kDatabaseError;
  }

  if (!settings_.SetLastUploadAttemptTime(now)) {
    return kDatabaseError;
  }

  return kNoError;
}

int CrashReportDatabase::CleanDatabase(time_t lockfile_ttl) {
  int removed = 0;
  time_t now = time(nullptr);

  DirectoryReader reader;
  const base::FilePath new_dir(base_dir_.Append(kNewDirectory));
  if (reader.Open(new_dir)) {
    base::FilePath filename;
    DirectoryReader::Result result;
    while ((result = reader.NextFile(&filename)) ==
           DirectoryReader::Result::kSuccess) {
      const base::FilePath filepath(new_dir.Append(filename));
      timespec filetime;
      if (!FileModificationTime(filepath, &filetime)) {
        continue;
      }
      if (filetime.tv_sec < now - lockfile_ttl) {
        LoggingRemoveFile(filepath);
      }
    }
  }

  removed += CleanReportsInState(kPending, lockfile_ttl);
  removed += CleanReportsInState(kCompleted, lockfile_ttl);
  return removed;
}

int CrashReportDatabase::CleanReportsInState(ReportState state, time_t lockfile_ttl) {
  const base::FilePath dir_path(base_dir_.Append(kReportDirectories[state]));
  DirectoryReader reader;
  if (!reader.Open(dir_path)) {
    return 0;
  }

  int removed = 0;
  base::FilePath filename;
  DirectoryReader::Result result;
  while ((result = reader.NextFile(&filename)) ==
         DirectoryReader::Result::kSuccess) {
    const base::FilePath::StringType extension(filename.FinalExtension());
    const base::FilePath filepath(dir_path.Append(filename));

    // Remove any report files without metadata
    if (extension.compare(kCrashReportExtension) == 0) {
      const base::FilePath metadata_path(
          ReplaceFinalExtension(filepath, kMetadataExtension));
      ScopedLockFile report_lock;
      if (report_lock.ResetAcquire(filepath) &&
          !IsRegularFile(metadata_path)) {
        LoggingRemoveFile(filepath);
        ++removed;
      }
      continue;
    }

    // Remove any metadata files without report files
    if (extension.compare(kMetadataExtension) == 0) {
      const base::FilePath report_path(
          ReplaceFinalExtension(filepath, kCrashReportExtension));
      ScopedLockFile report_lock;
      if (report_lock.ResetAcquire(report_path) &&
          !IsRegularFile(report_path)) {
        LoggingRemoveFile(filepath);
        ++removed;
      }
      continue;
    }

    // Remove any expired locks only if we can remove the report and metadata
    if (extension.compare(kLockExtension) == 0 &&
        ScopedLockFile::IsExpired(filepath, lockfile_ttl)) {
      const base::FilePath no_ext(filepath.RemoveFinalExtension());
      const base::FilePath report_path(no_ext.value() + kCrashReportExtension);
      const base::FilePath metadata_path(no_ext.value() + kMetadataExtension);
      if ((IsRegularFile(report_path) && !LoggingRemoveFile(report_path)) ||
          (IsRegularFile(metadata_path) && !LoggingRemoveFile(metadata_path))) {
        continue;
      }

      LoggingRemoveFile(filepath);
      ++removed;
      continue;
    }
  }

  return removed;
}

#if defined(OS_WIN)
void CrashReportDatabase::CleanOldWindowsData() {
  const base::FilePath metadata(
      base_dir_.Append(FILE_PATH_LITERAL("metadata")));
  if (IsRegularFile(metadata)) {
    LoggingRemoveFile(metadata);
  }

  const base::FilePath reports_dir(
      base_dir_.Append(FILE_PATH_LITERAL("reports")));
  if (!IsDirectory(reports_dir, /* allow_symlinks= */ false)) {
    return;
  }

  DirectoryReader reader;
  if (!reader.Open(reports_dir)) {
    return;
  }

  base::FilePath filename;
  DirectoryReader::Result result;
  while ((result = reader.NextFile(&filename)) ==
         DirectoryReader::Result::kSuccess) {
    LoggingRemoveFile(reports_dir.Append(filename));
  }
  LoggingRemoveDirectory(reports_dir);
}
#endif  // OS_WIN

// static
bool CrashReportDatabase::ReadMetadata(const base::FilePath& path,
                                       Report* report) {
  const base::FilePath metadata_path(
      ReplaceFinalExtension(path, kMetadataExtension));

  ScopedFileHandle handle(LoggingOpenFileForRead(metadata_path));
  if (!handle.is_valid()) {
    return false;
  }

  if (!report->uuid.InitializeFromString(
          path.BaseName().RemoveFinalExtension().value())) {
    LOG(ERROR) << "Couldn't interpret report uuid";
    return false;
  }

  ReportMetadata metadata;
  if (!LoggingReadFileExactly(handle.get(), &metadata, sizeof(metadata))) {
    return false;
  }

  if (metadata.version != ReportMetadata::kVersion) {
    LOG(ERROR) << "version mismatch";
    return false;
  }

  report->id = ReadRestOfFileAsString(handle.get());
  report->upload_attempts = metadata.upload_attempts;
  report->last_upload_attempt_time = metadata.last_upload_attempt_time;
  report->creation_time = metadata.creation_time;
  report->uploaded = (metadata.attributes & kAttributeUploaded) != 0;
  report->upload_explicitly_requested =
      (metadata.attributes & kAttributeUploadExplicitlyRequested) != 0;
  report->file_path = path;
  return true;
}

// static
bool CrashReportDatabase::CleaningReadMetadata(const base::FilePath& path,
                                               Report* report) {
  if (ReadMetadata(path, report)) {
    return true;
  }

  LoggingRemoveFile(path);
  LoggingRemoveFile(ReplaceFinalExtension(path, kMetadataExtension));
  return false;
}

// static
bool CrashReportDatabase::WriteNewMetadata(const base::FilePath& path) {
  const base::FilePath metadata_path(
      ReplaceFinalExtension(path, kMetadataExtension));

  ScopedFileHandle handle(LoggingOpenFileForWrite(metadata_path,
                                                  FileWriteMode::kCreateOrFail,
                                                  FilePermissions::kOwnerOnly));
  if (handle.get() == kInvalidFileHandle) {
    return false;
  }

  ReportMetadata metadata;
  metadata.creation_time = time(nullptr);

  return LoggingWriteFile(handle.get(), &metadata, sizeof(metadata));
}

// static
bool CrashReportDatabase::WriteMetadata(const base::FilePath& path,
                                        const Report& report) {
  const base::FilePath metadata_path(
      ReplaceFinalExtension(path, kMetadataExtension));

  ScopedFileHandle handle(
      LoggingOpenFileForWrite(metadata_path,
                              FileWriteMode::kTruncateOrCreate,
                              FilePermissions::kOwnerOnly));
  if (handle.get() == kInvalidFileHandle) {
    return false;
  }

  ReportMetadata metadata;
  metadata.creation_time = report.creation_time;
  metadata.last_upload_attempt_time = report.last_upload_attempt_time;
  metadata.upload_attempts = report.upload_attempts;
  metadata.attributes =
      (report.uploaded ? kAttributeUploaded : 0) |
      (report.upload_explicitly_requested ? kAttributeUploadExplicitlyRequested
                                          : 0);

  return LoggingWriteFile(handle.get(), &metadata, sizeof(metadata)) &&
         LoggingWriteFile(handle.get(), report.id.c_str(), report.id.size());
}

}  // namespace crashpad
