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

#if defined(OS_POSIX)
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "util/posix/scoped_dir.h"
#elif defined(OS_WIN)
#include <windows.h>
#include "base/strings/utf_string_conversions.h"
#endif

namespace crashpad {

namespace {

base::FilePath ReplaceFinalExtension(
    const base::FilePath& path,
    const base::FilePath::StringType extension) {
  return base::FilePath(path.RemoveFinalExtension().value() + extension);
}

bool LoggingMoveFile(const base::FilePath& source, const base::FilePath& dest) {
  DCHECK(!source.empty());
  DCHECK(!dest.empty());

#if defined(OS_POSIX)
  if (rename(source.value().c_str(), dest.value().c_str()) != 0) {
    PLOG(ERROR) << "rename " << source.value();
    return false;
  }
#elif defined(OS_WIN)
  if (!MoveFileEx(source.value().c_str(), dest.value().c_str(), 0)) {
    PLOG(ERROR) << "MoveFileEx" << base::UTF16ToUTF8(source.value());
    return false;
  }
#endif
  return true;
}

// Ensures that the node at |path| is a directory. If the |path| refers to a
// file, rather than a directory, returns false. Otherwise, returns true,
// indicating that |path| already was a directory.
bool EnsureDirectoryExists(const base::FilePath& path) {
#if defined(OS_POSIX)
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
#elif defined(OS_WIN)
  DWORD fileattr = GetFileAttributes(path.value().c_str());
  if (fileattr == INVALID_FILE_ATTRIBUTES) {
    PLOG(ERROR) << "GetFileAttributes " << base::UTF16ToUTF8(path.value());
    return false;
  }
  if ((fileattr & FILE_ATTRIBUTE_DIRECTORY) == 0) {
    LOG(ERROR) << "GetFileAttributes " << base::UTF16ToUTF8(path.value())
               << ": not a directory";
    return false;
  }
  return true;

#endif
}

// Ensures that the node at |path| is a directory, and creates it if it does
// not exist. If the |path| refers to a file, rather than a directory, or the
// directory could not be created, returns false. Otherwise, returns true,
// indicating that |path| already was or now is a directory.
bool CreateOrEnsureDirectoryExists(const base::FilePath& path) {
#if defined(OS_POSIX)
  if (mkdir(path.value().c_str(), 0755) == 0) {
    return true;
  }
  if (errno != EEXIST) {
    PLOG(ERROR) << "mkdir " << path.value();
    return false;
  }
#elif defined(OS_WIN)
  if (CreateDirectory(path.value().c_str(), nullptr))
    return true;
  if (GetLastError() != ERROR_ALREADY_EXISTS) {
    PLOG(ERROR) << "CreateDirectory " << base::UTF16ToUTF8(path.value());
    return false;
  }
#endif

  return EnsureDirectoryExists(path);
}

using OperationStatus = CrashReportDatabase::OperationStatus;

constexpr base::FilePath::CharType kSettings[] =
    FILE_PATH_LITERAL("settings.dat");

constexpr base::FilePath::CharType kCrashReportFileExtension[] =
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

enum ReportState : int32_t {
  kUninitialized = -1,
  //! \brief
  kNew,
  //! \brief Created and filled out by caller, owned by database.
  kPending,
  //! \brief Upload completed or skipped, owned by database.
  kCompleted,
};

enum {
  //! \brief Corresponds to uploaded bit of the report state.
  kAttributeUploaded = 1 << 0,

  //! \brief Corresponds to upload_explicity_requested bit of the report state.
  kAttributeUploadExplicitlyRequested = 1 << 1,
};

struct ReportMetadata {
  time_t creation_time;
  int64_t last_upload_attempt_time;
  int32_t upload_attempts;
  uint8_t attributes;
};

// Reads from the current file position to EOF and returns as a string of bytes.
std::string ReadRestOfFileAsString(FileHandle file) {
  FileOffset read_from = LoggingSeekFile(file, 0, SEEK_CUR);
  FileOffset end = LoggingSeekFile(file, 0, SEEK_END);
  FileOffset original = LoggingSeekFile(file, read_from, SEEK_SET);
  if (read_from == -1 || end == -1 || original == -1 || read_from == end)
    return std::string();
  DCHECK_EQ(read_from, original);
  DCHECK_GT(end, read_from);
  size_t data_length = static_cast<size_t>(end - read_from);
  std::string buffer(data_length, '\0');
  return LoggingReadFileExactly(file, &buffer[0], data_length) ? buffer
                                                               : std::string();
}

std::unique_ptr<CrashReportDatabase> InitializeInternal(
    const base::FilePath& path,
    bool may_create) {
  std::unique_ptr<CrashReportDatabase> database(new CrashReportDatabase(path));
  return database->Initialize(may_create)
             ? std::move(database)
             : std::unique_ptr<CrashReportDatabase>();
}

base::FilePath ReportPath(const base::FilePath& base_dir,
                          ReportState state,
                          const UUID& uuid) {
#if defined(OS_POSIX)
  return base_dir.Append(kReportDirectories[state])
      .Append(uuid.ToString() + kCrashReportFileExtension);
#elif defined(OS_WIN)
  return base_dir.Append(kReportDirectories[state])
      .Append(uuid.ToString16() + kCrashReportFileExtension);
#endif
}

CrashReportDatabase::ScopedLockFile ObtainReportLock(
    const base::FilePath& path) {
  base::FilePath lock_path(path.RemoveFinalExtension().value() +
                           kLockExtension);
  ScopedFileHandle lock_fd(LoggingOpenFileForWrite(
      lock_path, FileWriteMode::kCreateOrFail, FilePermissions::kOwnerOnly));
  if (lock_fd.get() == kInvalidFileHandle) {
    return CrashReportDatabase::ScopedLockFile();
  }
  return CrashReportDatabase::ScopedLockFile(lock_path);
}

}  // namespace

CrashReportDatabase::CrashReportDatabase(const base::FilePath& path)
    : base_dir_(path), settings_(base_dir_.Append(kSettings)), initialized_() {}

CrashReportDatabase::~CrashReportDatabase() {}

CrashReportDatabase::Report::Report()
    : uuid(),
      id(),
      creation_time(0),
      uploaded(false),
      last_upload_attempt_time(0),
      upload_attempts(0),
      upload_explicitly_requested(false) {}

CrashReportDatabase::Report::~Report() {}

CrashReportDatabase::NewReport::NewReport() : handle(), uuid() {}

CrashReportDatabase::NewReport::~NewReport() {}

CrashReportDatabase::UploadReport::UploadReport()
    : Report(), handle(), lock_file_(), database_() {}

CrashReportDatabase::UploadReport::~UploadReport() {
  if (database_) {
    database_->RecordUploadAttempt(this, false, std::string());
  }
}

// static
std::unique_ptr<CrashReportDatabase> CrashReportDatabase::Initialize(
    const base::FilePath& path) {
  return InitializeInternal(path, true);
}

// static
std::unique_ptr<CrashReportDatabase>
CrashReportDatabase::InitializeWithoutCreating(const base::FilePath& path) {
  return InitializeInternal(path, false);
}

bool CrashReportDatabase::Initialize(bool may_create) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  if (may_create) {
    if (!CreateOrEnsureDirectoryExists(base_dir_))
      return false;
  } else if (!EnsureDirectoryExists(base_dir_)) {
    return false;
  }

  for (size_t i = 0; i < arraysize(kReportDirectories); ++i) {
    if (!CreateOrEnsureDirectoryExists(
            base_dir_.Append(kReportDirectories[i]))) {
      return false;
    }
  }

  if (!settings_.Initialize())
    return false;

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

  std::unique_ptr<NewReport> new_report(new NewReport());
  if (!new_report->uuid.InitializeWithNew()) {
    return kFileSystemError;
  }
  LOG(INFO) << "Initialized uuid to " << new_report->uuid.ToString();

  const base::FilePath path = ReportPath(base_dir_, kNew, new_report->uuid);

  new_report->handle.reset(LoggingOpenFileForWrite(
      path, FileWriteMode::kCreateOrFail, FilePermissions::kOwnerOnly));
  if (new_report->handle.get() == kInvalidFileHandle) {
    return kDatabaseError;
  }

  new_report->file_.reset(path);

  report->reset(new_report.release());
  return kNoError;
}

OperationStatus CrashReportDatabase::FinishedWritingCrashReport(
    std::unique_ptr<NewReport>* report,
    UUID* uuid) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  base::FilePath path = ReportPath(base_dir_, kPending, (*report)->uuid);
  ScopedLockFile lock_file(ObtainReportLock(path));
  if (!lock_file.is_valid()) {
    return kDatabaseError;
  }

  if (!WriteNewMetadata(ReplaceFinalExtension(path, kMetadataExtension))) {
    return kDatabaseError;
  }

  if (!LoggingMoveFile((*report)->file_.get(), path)) {
    return kFileSystemError;
  }

  base::FilePath unused((*report)->file_.release());

  *uuid = (*report)->uuid;

  Metrics::CrashReportPending(Metrics::PendingReportReason::kNewlyCreated);
  Metrics::CrashReportSize((*report)->handle.get());

  report->reset();
  return kNoError;
}

OperationStatus CrashReportDatabase::LookUpCrashReport(const UUID& uuid,
                                                       Report* report) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  ScopedLockFile lock_file;
  base::FilePath path;
  OperationStatus os = LocateAndLockReport(uuid, &path, &lock_file);
  if (os != kNoError) {
    return os;
  }

  if (!ReadMetadata(path, report)) {
    return kDatabaseError;
  }

  return kNoError;
}

OperationStatus CrashReportDatabase::GetPendingReports(
    std::vector<Report>* reports) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return ReportsInDirectory(base_dir_.Append(kPendingDirectory), reports);
}

OperationStatus CrashReportDatabase::GetCompletedReports(
    std::vector<Report>* reports) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return ReportsInDirectory(base_dir_.Append(kCompletedDirectory), reports);
}

OperationStatus CrashReportDatabase::GetReportForUploading(
    const UUID& uuid,
    std::unique_ptr<const UploadReport>* report) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  std::unique_ptr<UploadReport> upload_report(new UploadReport);

  const base::FilePath path = ReportPath(base_dir_, kPending, uuid);
  upload_report->lock_file_.reset(ObtainReportLock(path).release());
  if (!upload_report->lock_file_.is_valid()) {
    return kBusyError;
  }

  if (!FileExists(path)) {
    return kReportNotFound;
  }

  if (!ReadMetadata(path, upload_report.get())) {
    return kDatabaseError;
  }

  upload_report->handle.reset(LoggingOpenFileForRead(path));
  if (upload_report->handle.get() == kInvalidFileHandle) {
    return kFileSystemError;
  }

  upload_report->database_ = this;
  report->reset(upload_report.release());
  return kNoError;
}

OperationStatus CrashReportDatabase::RecordUploadComplete(
    std::unique_ptr<const UploadReport>* report,
    const std::string& id) {
  OperationStatus os =
      RecordUploadAttempt(const_cast<UploadReport*>(report->get()), true, id);
  report->reset();
  return os;
}

OperationStatus CrashReportDatabase::SkipReportUpload(
    const UUID& uuid,
    Metrics::CrashSkippedReason reason) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  Metrics::CrashUploadSkipped(reason);

  const base::FilePath path(ReportPath(base_dir_, kPending, uuid));
  ScopedLockFile lock_file(ObtainReportLock(path));
  if (!lock_file.is_valid()) {
    return kBusyError;
  }

  if (!FileExists(path)) {
    return kReportNotFound;
  }

  base::FilePath new_path(ReportPath(base_dir_, kCompleted, uuid));
  ScopedLockFile new_lock_file(ObtainReportLock(new_path));
  if (!new_lock_file.is_valid()) {
    return kBusyError;
  }

  Report report;
  if (!ReadMetadata(path, &report)) {
    return kDatabaseError;
  }

  report.upload_explicitly_requested = false;
  if (!WriteMetadata(new_path, &report)) {
    return kDatabaseError;
  }

  if (!LoggingMoveFile(path, new_path)) {
    return kFileSystemError;
  }

  if (!LoggingDeleteFile(ReplaceFinalExtension(path, kMetadataExtension))) {
    return kDatabaseError;
  }

  return kNoError;
}

OperationStatus CrashReportDatabase::DeleteReport(const UUID& uuid) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  base::FilePath path;
  ScopedLockFile lock_file;
  OperationStatus os = LocateAndLockReport(uuid, &path, &lock_file);
  if (os != kNoError) {
    return os;
  }

  if (!LoggingDeleteFile(path)) {
    return kFileSystemError;
  }

  if (!LoggingDeleteFile(ReplaceFinalExtension(path, kMetadataExtension))) {
    return kDatabaseError;
  }

  return kNoError;
}

OperationStatus CrashReportDatabase::RequestUpload(const UUID& uuid) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  ScopedLockFile lock_file;
  base::FilePath path;
  OperationStatus os = LocateAndLockReport(uuid, &path, &lock_file);
  if (os != kNoError) {
    return os;
  }

  Report report;
  if (!ReadMetadata(path, &report)) {
    return kDatabaseError;
  }

  if (report.uploaded) {
    return kCannotRequestUpload;
  }

  report.upload_explicitly_requested = true;
  base::FilePath new_path = ReportPath(base_dir_, kPending, uuid);
  if (!LoggingMoveFile(path, new_path)) {
    return kFileSystemError;
  }

  if (!WriteMetadata(new_path, &report)) {
    return kDatabaseError;
  }

  if (new_path != path) {
    if (!LoggingDeleteFile(ReplaceFinalExtension(path, kMetadataExtension))) {
      return kDatabaseError;
    }
  }

  Metrics::CrashReportPending(Metrics::PendingReportReason::kUserInitiated);
  return kNoError;
}

OperationStatus CrashReportDatabase::LocateAndLockReport(
    const UUID& uuid,
    base::FilePath* path,
    ScopedLockFile* lock_file) {
  constexpr ReportState searchable_states[] = {kPending, kCompleted};

  for (const ReportState state : searchable_states) {
    base::FilePath local_path(ReportPath(base_dir_, state, uuid));
    ScopedLockFile local_lock(ObtainReportLock(local_path));
    if (!local_lock.is_valid()) {
      return kBusyError;
    }

    if (!FileExists(local_path)) {
      continue;
    }

    *path = local_path;
    lock_file->reset(local_lock.release());
    return kNoError;
  }

  return kReportNotFound;
}

CrashReportDatabase::OperationStatus CrashReportDatabase::ReportsInDirectory(
    const base::FilePath& dir_path,
    std::vector<Report>* reports) {
  DCHECK(reports->empty());

#if defined(OS_POSIX)
  DIR* dir = opendir(dir_path.value().c_str());
  if (!dir) {
    PLOG(ERROR) << "opendir";
    return CrashReportDatabase::kDatabaseError;
  }
  ScopedDIR scoped_dir(dir);

  dirent* dir_entry;
  while ((dir_entry = readdir(scoped_dir.get()))) {
    if (strncmp(dir_entry->d_name, ".", arraysize(dir_entry->d_name)) == 0 |
        strncmp(dir_entry->d_name, "..", arraysize(dir_entry->d_name)) == 0) {
      continue;
    }

    std::string filename(dir_entry->d_name);
    if (filename.size() > arraysize(kCrashReportFileExtension) &&
        filename.compare(
            filename.size() - (arraysize(kCrashReportFileExtension) - 1),
            arraysize(kCrashReportFileExtension),
            kCrashReportFileExtension) == 0) {
      base::FilePath path(dir_path.Append(filename));
      ScopedLockFile lock_file(ObtainReportLock(path));
      if (!lock_file.is_valid()) {
        continue;
      }

      Report report;
      if (!ReadMetadata(path, &report)) {
        LOG(WARNING) << "Failed to read report metadata for " << path.value();
        continue;
      }
      reports->push_back(report);
      reports->back().file_path = path;
    }
  }
  return kNoError;

#elif defined(OS_WIN)
  return kDatabaseError;  // TODO
#endif
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

    base::FilePath new_report_path =
        ReportPath(base_dir_, kCompleted, report->uuid);

    lock_file.reset(ObtainReportLock(new_report_path).release());
    if (!lock_file.is_valid()) {
      return kBusyError;
    }

    if (!LoggingMoveFile(report_path, new_report_path)) {
      return kFileSystemError;
    }

    LoggingDeleteFile(ReplaceFinalExtension(report_path, kMetadataExtension));
    report_path = new_report_path;
  }

  if (!WriteMetadata(report_path, report)) {
    return kDatabaseError;
  }

  if (!settings_.SetLastUploadAttemptTime(now)) {
    return kDatabaseError;
  }

  return kNoError;
}

// static
bool CrashReportDatabase::ReadMetadata(const base::FilePath& path,
                                       Report* report) {
  const base::FilePath metadata_path(
      ReplaceFinalExtension(path, kMetadataExtension));

  ScopedFileHandle handle(LoggingOpenFileForRead(metadata_path));
  if (handle.get() == kInvalidFileHandle) {
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

  report->id = ReadRestOfFileAsString(handle.get());
  report->creation_time = metadata.creation_time;
  report->last_upload_attempt_time = metadata.last_upload_attempt_time;
  report->upload_attempts = metadata.upload_attempts;
  report->uploaded = metadata.attributes & kAttributeUploaded;
  report->upload_explicitly_requested =
      metadata.attributes & kAttributeUploadExplicitlyRequested;
  report->file_path = path;
  return true;
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
  memset(&metadata, 0, sizeof(metadata));
  metadata.creation_time = time(nullptr);

  return LoggingWriteFile(handle.get(), &metadata, sizeof(metadata));
}

// static
bool CrashReportDatabase::WriteMetadata(const base::FilePath& path,
                                        Report* report) {
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
  metadata.creation_time = report->creation_time;
  metadata.last_upload_attempt_time = report->last_upload_attempt_time;
  metadata.upload_attempts = report->upload_attempts;
  metadata.attributes =
      (report->uploaded ? kAttributeUploaded : 0) |
      (report->upload_explicitly_requested ? kAttributeUploadExplicitlyRequested
                                           : 0);

  return LoggingWriteFile(handle.get(), &metadata, sizeof(metadata)) &&
         LoggingWriteFile(handle.get(), report->id.c_str(), report->id.size());
}

}  // namespace crashpad
