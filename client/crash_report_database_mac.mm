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

#include "client/crash_report_database_mac.h"

#include <errno.h>
#include <fcntl.h>
#import <Foundation/Foundation.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <uuid/uuid.h>

#include "base/logging.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/posix/eintr_wrapper.h"
#include "base/scoped_generic.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "client/settings.h"
#include "util/file/file_io.h"
#include "util/mac/xattr.h"
#include "util/misc/initialization_state_dcheck.h"
#include "util/misc/metrics.h"

namespace crashpad {

namespace {

constexpr char kWriteDirectory[] = "new";
constexpr char kUploadPendingDirectory[] = "pending";
constexpr char kCompletedDirectory[] = "completed";

constexpr char kSettings[] = "settings.dat";

constexpr const char* kReportDirectories[] = {
    kWriteDirectory,
    kUploadPendingDirectory,
    kCompletedDirectory,
};

constexpr char kCrashReportFileExtension[] = "dmp";

constexpr char kXattrUUID[] = "uuid";
constexpr char kXattrCollectorID[] = "id";
constexpr char kXattrCreationTime[] = "creation_time";
constexpr char kXattrIsUploaded[] = "uploaded";
constexpr char kXattrLastUploadTime[] = "last_upload_time";
constexpr char kXattrUploadAttemptCount[] = "upload_count";
constexpr char kXattrIsUploadExplicitlyRequested[] =
    "upload_explicitly_requested";

constexpr char kXattrDatabaseInitialized[] = "initialized";

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

// Creates a long database xattr name from the short constant name. These names
// have changed, and new_name determines whether the returned xattr name will be
// the old name or its new equivalent.
std::string XattrNameInternal(const base::StringPiece& name, bool new_name) {
  return base::StringPrintf(new_name ? "org.chromium.crashpad.database.%s"
                                     : "com.googlecode.crashpad.%s",
                            name.data());
}

}  // namespace

CrashReportDatabaseMac::CrashReportDatabaseMac()
    : CrashReportDatabase(), xattr_new_names_(false) {}

CrashReportDatabaseMac::~CrashReportDatabaseMac() {}

bool CrashReportDatabaseMac::Initialize(const base::FilePath& path,
                                        bool may_create) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);
  base_dir_ = path;

  // Check if the database already exists.
  if (may_create) {
    if (!CreateOrEnsureDirectoryExists(base_dir_)) {
      return false;
    }
  } else if (!EnsureDirectoryExists(base_dir_)) {
    return false;
  }

  // Create the three processing directories for the database.
  for (size_t i = 0; i < arraysize(kReportDirectories); ++i) {
    if (!CreateOrEnsureDirectoryExists(base_dir_.Append(kReportDirectories[i])))
      return false;
  }

  if (!settings_.Initialize(base_dir_.Append(kSettings)))
    return false;

  // Do an xattr operation as the last step, to ensure the filesystem has
  // support for them. This xattr also serves as a marker for whether the
  // database uses old or new xattr names.
  bool value;
  if (ReadXattrBool(base_dir_,
                    XattrNameInternal(kXattrDatabaseInitialized, true),
                    &value) == XattrStatus::kOK &&
      value) {
    xattr_new_names_ = true;
  } else if (ReadXattrBool(base_dir_,
                           XattrNameInternal(kXattrDatabaseInitialized, false),
                           &value) == XattrStatus::kOK &&
             value) {
    xattr_new_names_ = false;
  } else {
    xattr_new_names_ = true;
    if (!WriteXattrBool(base_dir_, XattrName(kXattrDatabaseInitialized), true))
      return false;
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

CrashReportDatabase::OperationStatus
CrashReportDatabaseMac::PrepareNewCrashReport(
    std::unique_ptr<NewReport>* out_report) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  std::unique_ptr<NewReport> report(new NewReport());

  uuid_t uuid_gen;
  uuid_generate(uuid_gen);
  report->uuid_.InitializeFromBytes(uuid_gen);

  const base::FilePath path =
      base_dir_.Append(kWriteDirectory)
          .Append(report->uuid_.ToString() + "." + kCrashReportFileExtension);

  if (!report->writer_->Open(
          path, FileWriteMode::kCreateOrFail, FilePermissions::kOwnerOnly)) {
    return kFileSystemError;
  }
  report->file_remover_.reset(path);

  // TODO(rsesek): Potentially use an fsetxattr() here instead.
  if (!WriteXattr(path, XattrName(kXattrUUID), report->uuid_.ToString())) {
    return kDatabaseError;
  }

  out_report->reset(report.release());

  return kNoError;
}

CrashReportDatabase::OperationStatus
CrashReportDatabaseMac::FinishedWritingCrashReport(
    std::unique_ptr<NewReport> report,
    UUID* uuid) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  const base::FilePath& path = report->file_remover_.get();

  // Get the report's UUID to return.
  std::string uuid_string;
  if (ReadXattr(path, XattrName(kXattrUUID), &uuid_string) !=
          XattrStatus::kOK ||
      !uuid->InitializeFromString(uuid_string)) {
    LOG(ERROR) << "Failed to read UUID for crash report " << path.value();
    return kDatabaseError;
  }

  if (*uuid != report->uuid_) {
    LOG(ERROR) << "UUID mismatch for crash report " << path.value();
    return kDatabaseError;
  }

  // Record the creation time of this report.
  if (!WriteXattrTimeT(path, XattrName(kXattrCreationTime), time(nullptr))) {
    return kDatabaseError;
  }

  FileOffset size = report->writer_->Seek(0, SEEK_END);

  // Move the report to its new location for uploading.
  base::FilePath new_path =
      base_dir_.Append(kUploadPendingDirectory).Append(path.BaseName());
  if (rename(path.value().c_str(), new_path.value().c_str()) != 0) {
    PLOG(ERROR) << "rename " << path.value() << " to " << new_path.value();
    return kFileSystemError;
  }
  ignore_result(report->file_remover_.release());

  Metrics::CrashReportPending(Metrics::PendingReportReason::kNewlyCreated);
  Metrics::CrashReportSize(size);

  return kNoError;
}

CrashReportDatabase::OperationStatus
CrashReportDatabaseMac::LookUpCrashReport(const UUID& uuid,
                                          CrashReportDatabase::Report* report) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  base::FilePath path = LocateCrashReport(uuid, kReportStateAny);
  if (path.empty())
    return kReportNotFound;

  base::ScopedFD lock(ObtainReportLock(path));
  if (!lock.is_valid())
    return kBusyError;

  *report = Report();
  report->file_path = path;
  if (!ReadReportMetadataLocked(path, report))
    return kDatabaseError;

  return kNoError;
}

CrashReportDatabase::OperationStatus
CrashReportDatabaseMac::GetPendingReports(
    std::vector<CrashReportDatabase::Report>* reports) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  return ReportsInDirectory(base_dir_.Append(kUploadPendingDirectory), reports);
}

CrashReportDatabase::OperationStatus
CrashReportDatabaseMac::GetCompletedReports(
    std::vector<CrashReportDatabase::Report>* reports) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  return ReportsInDirectory(base_dir_.Append(kCompletedDirectory), reports);
}

CrashReportDatabase::OperationStatus
CrashReportDatabaseMac::GetReportForUploading(
    const UUID& uuid,
    std::unique_ptr<const UploadReport>* report) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  auto upload_report = std::make_unique<UploadReportMac>();

  upload_report->file_path = LocateCrashReport(uuid, kReportStatePending);
  if (upload_report->file_path.empty())
    return kReportNotFound;

  base::ScopedFD lock(ObtainReportLock(upload_report->file_path));
  if (!lock.is_valid())
    return kBusyError;

  if (!ReadReportMetadataLocked(upload_report->file_path, upload_report.get()))
    return kDatabaseError;

  if (!upload_report->reader_->Open(upload_report->file_path)) {
    return kFileSystemError;
  }

  upload_report->database_ = this;
  upload_report->lock_fd.reset(lock.release());
  report->reset(upload_report.release());
  return kNoError;
}

CrashReportDatabase::OperationStatus CrashReportDatabaseMac::SkipReportUpload(
    const UUID& uuid,
    Metrics::CrashSkippedReason reason) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  Metrics::CrashUploadSkipped(reason);

  base::FilePath report_path = LocateCrashReport(uuid, kReportStatePending);
  if (report_path.empty())
    return kReportNotFound;

  base::ScopedFD lock(ObtainReportLock(report_path));
  if (!lock.is_valid())
    return kBusyError;

  return MarkReportCompletedLocked(report_path, nullptr);
}

CrashReportDatabase::OperationStatus
CrashReportDatabaseMac::GetReportForReading(
    const UUID& uuid,
    std::unique_ptr<const ReadReport>* report) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  auto read_report = std::make_unique<ReadReportMac>();

  read_report->file_path =
      LocateCrashReport(uuid, kReportStatePending | kReportStateCompleted);
  if (read_report->file_path.empty())
    return kReportNotFound;

  base::ScopedFD lock(ObtainReportLock(read_report->file_path));
  if (!lock.is_valid())
    return kBusyError;

  if (!ReadReportMetadataLocked(read_report->file_path, read_report.get()))
    return kDatabaseError;

  if (!read_report->reader_->Open(read_report->file_path)) {
    return kFileSystemError;
  }

  read_report->lock_fd.reset(lock.release());
  report->reset(read_report.release());
  return kNoError;
}

CrashReportDatabase::OperationStatus CrashReportDatabaseMac::DeleteReport(
    const UUID& uuid) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  base::FilePath report_path = LocateCrashReport(uuid, kReportStateAny);
  if (report_path.empty())
    return kReportNotFound;

  base::ScopedFD lock(ObtainReportLock(report_path));
  if (!lock.is_valid())
    return kBusyError;

  if (unlink(report_path.value().c_str()) != 0) {
    PLOG(ERROR) << "unlink " << report_path.value();
    return kFileSystemError;
  }

  return kNoError;
}

CrashReportDatabase::OperationStatus CrashReportDatabaseMac::RequestUpload(
    const UUID& uuid) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  base::FilePath report_path =
      LocateCrashReport(uuid, kReportStatePending | kReportStateCompleted);
  if (report_path.empty())
    return kReportNotFound;

  base::ScopedFD lock(ObtainReportLock(report_path));
  if (!lock.is_valid())
    return kBusyError;

  // If the crash report has already been uploaded, don't request new upload.
  bool uploaded = false;
  XattrStatus status =
      ReadXattrBool(report_path, XattrName(kXattrIsUploaded), &uploaded);
  if (status == XattrStatus::kOtherError)
    return kDatabaseError;
  if (uploaded)
    return kCannotRequestUpload;

  // Mark the crash report as having upload explicitly requested by the user,
  // and move it to the pending state.
  if (!WriteXattrBool(
          report_path, XattrName(kXattrIsUploadExplicitlyRequested), true)) {
    return kDatabaseError;
  }

  base::FilePath new_path =
      base_dir_.Append(kUploadPendingDirectory).Append(report_path.BaseName());
  if (rename(report_path.value().c_str(), new_path.value().c_str()) != 0) {
    PLOG(ERROR) << "rename " << report_path.value() << " to "
                << new_path.value();
    return kFileSystemError;
  }

  Metrics::CrashReportPending(Metrics::PendingReportReason::kUserInitiated);

  return kNoError;
}

int CrashReportDatabaseMac::CleanDatabase(time_t ttl) {
  return 0;
}

CrashReportDatabaseMac::UploadReportMac::UploadReportMac() : UploadReport() {}

CrashReportDatabaseMac::UploadReportMac::~UploadReportMac() = default;

CrashReportDatabaseMac::ReadReportMac::ReadReportMac() : ReadReport() {}

CrashReportDatabaseMac::ReadReportMac::~ReadReportMac() = default;

CrashReportDatabase::OperationStatus
CrashReportDatabaseMac::RecordUploadAttempt(UploadReport* report,
                                            bool successful,
                                            const std::string& id) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  Metrics::CrashUploadAttempted(successful);

  DCHECK(report);
  DCHECK(successful || id.empty());

  base::FilePath report_path =
      LocateCrashReport(report->uuid, kReportStatePending);
  if (report_path.empty())
    return kReportNotFound;

  if (successful) {
    CrashReportDatabase::OperationStatus os =
        MarkReportCompletedLocked(report_path, &report_path);
    if (os != kNoError)
      return os;
  }

  if (!WriteXattrBool(report_path, XattrName(kXattrIsUploaded), successful)) {
    return kDatabaseError;
  }
  if (!WriteXattr(report_path, XattrName(kXattrCollectorID), id)) {
    return kDatabaseError;
  }

  time_t now = time(nullptr);
  if (!WriteXattrTimeT(report_path, XattrName(kXattrLastUploadTime), now)) {
    return kDatabaseError;
  }

  int upload_attempts = 0;
  std::string name = XattrName(kXattrUploadAttemptCount);
  if (ReadXattrInt(report_path, name, &upload_attempts) ==
          XattrStatus::kOtherError) {
    return kDatabaseError;
  }
  if (!WriteXattrInt(report_path, name, ++upload_attempts)) {
    return kDatabaseError;
  }

  if (!settings_.SetLastUploadAttemptTime(now)) {
    return kDatabaseError;
  }

  return kNoError;
}

base::FilePath CrashReportDatabaseMac::LocateCrashReport(
    const UUID& uuid,
    uint8_t desired_state) {
  const std::string target_uuid = uuid.ToString();

  std::vector<std::string> report_directories;
  if (desired_state & kReportStateWrite) {
    report_directories.push_back(kWriteDirectory);
  }
  if (desired_state & kReportStatePending) {
    report_directories.push_back(kUploadPendingDirectory);
  }
  if (desired_state & kReportStateCompleted) {
    report_directories.push_back(kCompletedDirectory);
  }

  for (const std::string& report_directory : report_directories) {
    base::FilePath path =
        base_dir_.Append(report_directory)
                 .Append(target_uuid + "." + kCrashReportFileExtension);

    // Test if the path exists.
    struct stat st;
    if (lstat(path.value().c_str(), &st)) {
      continue;
    }

    // Check that the UUID of the report matches.
    std::string uuid_string;
    if (ReadXattr(path, XattrName(kXattrUUID),
                  &uuid_string) == XattrStatus::kOK &&
        uuid_string == target_uuid) {
      return path;
    }
  }

  return base::FilePath();
}



// static
base::ScopedFD CrashReportDatabaseMac::ObtainReportLock(
    const base::FilePath& path) {
  int fd = HANDLE_EINTR(
      open(path.value().c_str(),
           O_RDONLY | O_NONBLOCK | O_EXLOCK | O_NOCTTY | O_CLOEXEC));
  PLOG_IF(ERROR, fd < 0) << "open lock " << path.value();
  return base::ScopedFD(fd);
}

bool CrashReportDatabaseMac::ReadReportMetadataLocked(
    const base::FilePath& path, Report* report) {
  std::string uuid_string;
  if (ReadXattr(path, XattrName(kXattrUUID),
                &uuid_string) != XattrStatus::kOK ||
      !report->uuid.InitializeFromString(uuid_string)) {
    return false;
  }

  if (ReadXattrTimeT(path, XattrName(kXattrCreationTime),
                     &report->creation_time) != XattrStatus::kOK) {
    return false;
  }

  report->id = std::string();
  if (ReadXattr(path, XattrName(kXattrCollectorID),
                &report->id) == XattrStatus::kOtherError) {
    return false;
  }

  report->uploaded = false;
  if (ReadXattrBool(path, XattrName(kXattrIsUploaded),
                    &report->uploaded) == XattrStatus::kOtherError) {
    return false;
  }

  report->last_upload_attempt_time = 0;
  if (ReadXattrTimeT(path, XattrName(kXattrLastUploadTime),
                     &report->last_upload_attempt_time) ==
          XattrStatus::kOtherError) {
    return false;
  }

  report->upload_attempts = 0;
  if (ReadXattrInt(path, XattrName(kXattrUploadAttemptCount),
                   &report->upload_attempts) == XattrStatus::kOtherError) {
    return false;
  }

  report->upload_explicitly_requested = false;
  if (ReadXattrBool(path,
                    XattrName(kXattrIsUploadExplicitlyRequested),
                    &report->upload_explicitly_requested) ==
      XattrStatus::kOtherError) {
    return false;
  }

  return true;
}

CrashReportDatabase::OperationStatus CrashReportDatabaseMac::ReportsInDirectory(
    const base::FilePath& path,
    std::vector<CrashReportDatabase::Report>* reports) {
  base::mac::ScopedNSAutoreleasePool pool;

  DCHECK(reports->empty());

  NSError* error = nil;
  NSArray* paths = [[NSFileManager defaultManager]
      contentsOfDirectoryAtPath:base::SysUTF8ToNSString(path.value())
                          error:&error];
  if (error) {
    LOG(ERROR) << "Failed to enumerate reports in directory " << path.value()
               << ": " << [[error description] UTF8String];
    return kFileSystemError;
  }

  reports->reserve([paths count]);
  for (NSString* entry in paths) {
    Report report;
    report.file_path = path.Append([entry fileSystemRepresentation]);
    base::ScopedFD lock(ObtainReportLock(report.file_path));
    if (!lock.is_valid())
      continue;

    if (!ReadReportMetadataLocked(report.file_path, &report)) {
      LOG(WARNING) << "Failed to read report metadata for "
                   << report.file_path.value();
      continue;
    }
    reports->push_back(report);
  }

  return kNoError;
}

std::string CrashReportDatabaseMac::XattrName(const base::StringPiece& name) {
  return XattrNameInternal(name, xattr_new_names_);
}

CrashReportDatabase::OperationStatus
CrashReportDatabaseMac::MarkReportCompletedLocked(
    const base::FilePath& report_path,
    base::FilePath* out_path) {
  if (RemoveXattr(report_path, XattrName(kXattrIsUploadExplicitlyRequested)) ==
      XattrStatus::kOtherError) {
    return kDatabaseError;
  }

  base::FilePath new_path =
      base_dir_.Append(kCompletedDirectory).Append(report_path.BaseName());
  if (rename(report_path.value().c_str(), new_path.value().c_str()) != 0) {
    PLOG(ERROR) << "rename " << report_path.value() << " to "
                << new_path.value();
    return kFileSystemError;
  }

  if (out_path)
    *out_path = new_path;
  return kNoError;
}

}  // namespace crashpad
