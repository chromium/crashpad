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

#include <stdint.h>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "client/crash_report_database.h"

namespace crashpad {

//! \brief A CrashReportDatabase that uses HFS+ extended attributes to store
//!     report metadata.
//!
//! The database maintains three directories of reports: `"new"` to hold crash
//! reports that are in the process of being written, `"completed"` to hold
//! reports that have been written and are awaiting upload, and `"uploaded"` to
//! hold reports successfully uploaded to a collection server. If the user has
//! opted out of report collection, reports will still be written and moved
//! to the completed directory, but they just will not be uploaded.
//!
//! The database stores its metadata in extended filesystem attributes. To
//! ensure safe access, the report file is locked using `O_EXLOCK` during all
//! extended attribute operations. The lock should be obtained using
//! ObtainReportLock().
class CrashReportDatabaseMac : public CrashReportDatabase {
 public:
  CrashReportDatabaseMac();
  virtual ~CrashReportDatabaseMac();

  // CrashReportDatabase:
  bool Initialize(const base::FilePath& path, bool may_create) override;
  OperationStatus PrepareNewCrashReport(
      std::unique_ptr<NewReport>* report) override;
  OperationStatus FinishedWritingCrashReport(std::unique_ptr<NewReport> report,
                                             UUID* uuid) override;
  OperationStatus LookUpCrashReport(const UUID& uuid, Report* report) override;
  OperationStatus GetPendingReports(std::vector<Report>* reports) override;
  OperationStatus GetCompletedReports(std::vector<Report>* reports) override;
  OperationStatus GetReportForUploading(
      const UUID& uuid,
      std::unique_ptr<const UploadReport>* report) override;
  OperationStatus SkipReportUpload(const UUID& uuid,
                                   Metrics::CrashSkippedReason reason) override;
  OperationStatus GetReportForReading(
      const UUID& uuid,
      std::unique_ptr<const ReadReport>* report) override;
  OperationStatus DeleteReport(const UUID& uuid) override;
  OperationStatus RequestUpload(const UUID& uuid) override;

 private:
  //! \brief Report states for use with LocateCrashReport().
  //!
  //! ReportState may be considered to be a bitfield.
  enum ReportState : uint8_t {
    kReportStateWrite = 1 << 0,  // in kWriteDirectory
    kReportStatePending = 1 << 1,  // in kUploadPendingDirectory
    kReportStateCompleted = 1 << 2,  // in kCompletedDirectory
    kReportStateAny =
        kReportStateWrite | kReportStatePending | kReportStateCompleted,
  };

  //! \brief A private extension of the Report class that maintains bookkeeping
  //!    information of the database.
  struct UploadReportMac : public UploadReport {
    UploadReportMac();
    ~UploadReportMac() override;

    //! \brief Stores the flock of the file for the duration of
    //!     GetReportForUploading() and RecordUploadComplete().
    base::ScopedFD lock_fd;
  };

  struct ReadReportMac : public ReadReport {
    ReadReportMac();
    ~ReadReportMac() override;

    base::ScopedFD lock_fd;
  };

  //! \see CrashReportDatabase::RecordUploadAttempt
  OperationStatus RecordUploadAttempt(UploadReport* report,
                                      bool successful,
                                      const std::string& id) override;

  //! \brief Locates a crash report in the database by UUID.
  //!
  //! \param[in] uuid The UUID of the crash report to locate.
  //! \param[in] desired_state The state of the report to locate, composed of
  //!     ReportState values.
  //!
  //! \return The full path to the report file, or an empty path if it cannot be
  //!     found.
  base::FilePath LocateCrashReport(const UUID& uuid, uint8_t desired_state);

  //! \brief Obtains an exclusive advisory lock on a file.
  //!
  //! The flock is used to prevent cross-process concurrent metadata reads or
  //! writes. While xattrs do not observe the lock, if the lock-then-mutate
  //! protocol is observed by all clients of the database, it still enforces
  //! synchronization.
  //!
  //! This does not block, and so callers must ensure that the lock is valid
  //! after calling.
  //!
  //! \param[in] path The path of the file to lock.
  //!
  //! \return A scoped lock object. If the result is not valid, an error is
  //!     logged.
  static base::ScopedFD ObtainReportLock(const base::FilePath& path);

  //! \brief Reads all the database xattrs from a file into a Report. The file
  //!     must be locked with ObtainReportLock.
  //!
  //! \param[in] path The path of the report.
  //! \param[out] report The object into which data will be read.
  //!
  //! \return `true` if all the metadata was read successfully, `false`
  //!     otherwise.
  bool ReadReportMetadataLocked(const base::FilePath& path, Report* report);

  //! \brief Reads the metadata from all the reports in a database subdirectory.
  //!      Invalid reports are skipped.
  //!
  //! \param[in] path The database subdirectory path.
  //! \param[out] reports An empty vector of reports, which will be filled.
  //!
  //! \return The operation status code.
  OperationStatus ReportsInDirectory(const base::FilePath& path,
                                     std::vector<Report>* reports);

  //! \brief Creates a database xattr name from the short constant name.
  //!
  //! \param[in] name The short name of the extended attribute.
  //!
  //! \return The long name of the extended attribute.
  std::string XattrName(const base::StringPiece& name);

  //! \brief Marks a report with a given path as completed.
  //!
  //! Assumes that the report is locked.
  //!
  //! \param[in] report_path The path of the file to mark completed.
  //! \param[out] out_path The path of the new file. This parameter is optional.
  //!
  //! \return The operation status code.
  CrashReportDatabase::OperationStatus MarkReportCompletedLocked(
      const base::FilePath& report_path,
      base::FilePath* out_path);

  bool xattr_new_names_;

  DISALLOW_COPY_AND_ASSIGN(CrashReportDatabaseMac);
};

}  // namespace crashpad
