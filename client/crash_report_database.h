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

#ifndef CRASHPAD_CLIENT_CRASH_REPORT_DATABASE_H_
#define CRASHPAD_CLIENT_CRASH_REPORT_DATABASE_H_

#include <time.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "client/settings.h"
#include "util/file/file_reader.h"
#include "util/file/file_writer.h"
#include "util/file/scoped_remove_file.h"
#include "util/misc/initialization_state_dcheck.h"
#include "util/misc/metrics.h"
#include "util/misc/uuid.h"

namespace crashpad {

//! \brief Manages a collection of crash report files and metadata associated
//!     with the crash reports.
//!
//! All Report objects that are returned by this class are logically const.
//! They are snapshots of the database at the time the query was run, and the
//! data returned is liable to change after the query is executed.
//!
//! The lifecycle of a crash report has three stages:
//!
//!   1. New: A crash report is created with PrepareNewCrashReport(), the
//!      the client then writes the report, and then calls
//!      FinishedWritingCrashReport() to make the report Pending.
//!   2. Pending: The report has been written but has not been locally
//!      processed, or it was has been brought back from 'Completed' state by
//!      user request.
//!   3. Completed: The report has been locally processed, either by uploading
//!      it to a collection server and calling RecordUploadComplete(), or by
//!      calling SkipReportUpload().
//!
//! Methods which lock reports preclude those reports from being read or
//! modified by any other methods in this class.
class CrashReportDatabase {
 public:
  CrashReportDatabase();
  virtual ~CrashReportDatabase();

  //! \brief A crash report record.
  //!
  //! This represents the metadata for a crash report.
  struct Report {
    Report();
    virtual ~Report();

    //! A unique identifier by which this report will always be known to the
    //! database.
    UUID uuid;

    //! An identifier issued to this crash report by a collection server.
    std::string id;

    //! The time at which the report was generated.
    time_t creation_time;

    //! Whether this crash report was successfully uploaded to a collection
    //! server.
    bool uploaded;

    //! The last timestamp at which an attempt was made to submit this crash
    //! report to a collection server. If this is zero, then the report has
    //! never been uploaded. If #uploaded is true, then this timestamp is the
    //! time at which the report was uploaded, and no other attempts to upload
    //! this report will be made.
    time_t last_upload_attempt_time;

    //! The number of times an attempt was made to submit this report to
    //! a collection server. If this is more than zero, then
    //! #last_upload_attempt_time will be set to the timestamp of the most
    //! recent attempt.
    int upload_attempts;

    //! Whether this crash report was explicitly requested by user to be
    //! uploaded. This can be true only if report is in the 'pending' state.
    bool upload_explicitly_requested;

   private:
    base::FilePath file_path;

    friend class CrashReportDatabase;

    // These things may use file_path, but the database makes no guarantees that
    // the path will remain valid.
    friend class DatabaseSizePruneCondition;
    friend void internal::ShowReport(const Report&, size_t, bool);
  };

  //! \brief A crash report that is in the process of being written.
  //!
  //! An instance of this struct should be created via PrepareNewCrashReport().
  struct NewReport {
    NewReport();
    ~NewReport();

    //! An open FileWriter with which to write the report.
    FileWriter writer;

    //! A unique identifier by which this report will always be known to the
    //! database.
    UUID uuid;

   private:
    ScopedRemoveFile file_;

    friend class CrashReportDatabase;

    DISALLOW_COPY_AND_ASSIGN(NewReport);
  };

  using ScopedLockFile = ScopedRemoveFile;

  //! \brief A crash report that is in the process of being uploaded.
  //!
  //! An instance of this struct should be created via GetReportForUploading().
  struct UploadReport : public Report {
    UploadReport();
    ~UploadReport();

    // An open FileReader with which to read the report.
    FileReader reader;

   private:
    ScopedLockFile lock_file_;
    CrashReportDatabase* database_;

    friend class CrashReportDatabase;
  };

  //! \brief A crash report that provides an opened reader to read the report.
  //!
  //! An instance of this struct should be created via GetReportForReading().
  struct ReadReport : public Report {
    ReadReport();
    ~ReadReport();

    // An open FileReader with which to read the report.
    FileReader reader;

   private:
    ScopedLockFile lock_file_;

    friend class CrashReportDatabase;
  };

  //! \brief The result code for operations performed on a database.
  enum OperationStatus {
    //! \brief No error occurred.
    kNoError = 0,

    //! \brief The report that was requested could not be located.
    //!
    //! This may occur when the report is present in the database but not in a
    //! state appropriate for the requested operation, for example, if
    //! GetReportForUploading() is called to obtain report that’s already in the
    //! completed state.
    kReportNotFound,

    //! \brief An error occured while performing a file operation on a crash
    //!     report.
    //!
    //! A database is responsible for managing both the metadata about a report
    //! and the actual crash report itself. This error is returned when an
    //! error occurred when managing the report file. Additional information
    //! will be logged.
    kFileSystemError,

    //! \brief An error occured while recording metadata for a crash report or
    //!     database-wide settings.
    //!
    //! A database is responsible for managing both the metadata about a report
    //! and the actual crash report itself. This error is returned when an
    //! error occurred when managing the metadata about a crash report or
    //! database-wide settings. Additional information will be logged.
    kDatabaseError,

    //! \brief The operation could not be completed because a concurrent
    //!     operation affecting the report is occurring.
    kBusyError,

    //! \brief The report cannot be uploaded by user request as it has already
    //!     been uploaded.
    kCannotRequestUpload,
  };

  //! \brief Initializes the database.
  //!
  //! \param[in] path A path specifying the directory at which to open or create
  //!     the database.
  //! \param[in] may_create If `true`, the database directory will be created if
  //!     it does not already exist.
  //!
  //! \return `true` if the database was successfully initialized. Otherwise
  //!     `false` with a message logged.
  bool Initialize(const base::FilePath& path, bool may_create);

  //! \brief Returns the Settings object for this database.
  //!
  //! \return A weak pointer to the Settings object, which is owned by the
  //!     database.
  virtual Settings* GetSettings();

  //! \brief Creates a record of a new crash report.
  //!
  //! Callers should write the crash report using the FileWriter provided.
  //! Callers should then call FinishedWritingCrashReport() to complete report
  //! creation. If FinishedWritingCrashReport() is not called, the report will
  //! be removed from the database when \a report is destroyed.
  //!
  //! \param[out] report A NewReport object containing a FileWriter with which
  //!     to write the report data. Only valid if this returns #kNoError.
  //!
  //! \return The operation status code.
  virtual OperationStatus PrepareNewCrashReport(
      std::unique_ptr<NewReport>* report);

  //! \brief Informs the database that a crash report has been successfully
  //!     written.
  //!
  //! \param[in] report A NewReport obtained with PrepareNewCrashReport(). The
  //!     NewReport object will be invalidated as part of this call.
  //! \param[out] uuid The UUID of this crash report.
  //!
  //! \return The operation status code.
  virtual OperationStatus FinishedWritingCrashReport(
      std::unique_ptr<NewReport>* report,
      UUID* uuid);

  //! \brief Returns the crash report record for the unique identifier.
  //!
  //! \param[in] uuid The crash report record unique identifier.
  //! \param[out] report A crash report record. Only valid if this returns
  //!     #kNoError.
  //!
  //! \return The operation status code.
  virtual OperationStatus LookUpCrashReport(const UUID& uuid, Report* report);

  //! \brief Returns a list of crash report records that have not been uploaded.
  //!
  //! \param[out] reports A list of crash report record objects. This must be
  //!     empty on entry. Only valid if this returns #kNoError.
  //!
  //! \return The operation status code.
  virtual OperationStatus GetPendingReports(std::vector<Report>* reports);

  //! \brief Returns a list of crash report records that have been completed,
  //!     either by being uploaded or by skipping upload.
  //!
  //! \param[out] reports A list of crash report record objects. This must be
  //!     empty on entry. Only valid if this returns #kNoError.
  //!
  //! \return The operation status code.
  virtual OperationStatus GetCompletedReports(std::vector<Report>* reports);

  //! \brief Obtains and locks a report object for uploading to a collection
  //!     server.
  //!
  //! Callers should upload the crash report using the FileReader provided.
  //! Callers should then call RecordUploadComplete() to record a successful
  //! upload. If RecordUploadComplete() is not called, the upload attempt will
  //! be recorded as unsuccessful and the report lock released when \a report is
  //! destroyed.
  //!
  //! \param[in] uuid The unique identifier for the crash report record.
  //! \param[out] report A crash report record for the report to be uploaded.
  //!     Only valid if this returns #kNoError.
  //!
  //! \return The operation status code.
  virtual OperationStatus GetReportForUploading(
      const UUID& uuid,
      std::unique_ptr<const UploadReport>* report);

  //! \brief Records a successful upload for a report and updates the last
  //!     upload attempt time as returned by
  //!     Settings::GetLastUploadAttemptTime().
  //!
  //! \param[in] report A UploadReport object obtained from
  //!     GetReportForUploading(). The UploadReport object will be invalidated
  //!     and the report unlocked as part of this call.
  //! \param[in] id The possibly empty identifier assigned to this crash report
  //!     by the collection server.
  //!
  //! \return The operation status code.
  virtual OperationStatus RecordUploadComplete(
      std::unique_ptr<const UploadReport>* report,
      const std::string& id);

  //! \brief Moves a report from the pending state to the completed state, but
  //!     without the report being uploaded.
  //!
  //! This can be used if the user has disabled crash report collection, but
  //! crash generation is still enabled in the product.
  //!
  //! \param[in] uuid The unique identifier for the crash report record.
  //! \param[in] reason The reason the report upload is being skipped for
  //!     metrics tracking purposes.
  //!
  //! \return The operation status code.
  virtual OperationStatus SkipReportUpload(const UUID& uuid,
                                           Metrics::CrashSkippedReason reason);

  //! \brief Obtains and locks a report object for reading.
  //!
  //! The report lock will be released when \a report is destroyed.
  //!
  //! \param[in] The unique identifier for the crash report record.
  //! \param[out] report A crash report record for the report to be uploaded.
  //!     Only valid if this returns #kNoError.
  //!
  //! \return the operation status code.
  virtual OperationStatus GetReportForReading(
      const UUID& uuid,
      std::unique_ptr<const ReadReport>* report);

  //! \brief Deletes a crash report file and its associated metadata.
  //!
  //! \param[in] uuid The UUID of the report to delete.
  //!
  //! \return The operation status code.
  virtual OperationStatus DeleteReport(const UUID& uuid);

  //! \brief Marks a crash report as explicitly requested to be uploaded by the
  //!     user and moves it to 'pending' state.
  //!
  //! \param[in] uuid The unique identifier for the crash report record.
  //!
  //! \return The operation status code.
  virtual OperationStatus RequestUpload(const UUID& uuid);

 private:
  OperationStatus LocateAndLockReport(const UUID& uuid,
                                      base::FilePath* path,
                                      ScopedLockFile* lock_file);

  OperationStatus ReportsInDirectory(const base::FilePath& dir_path,
                                     std::vector<Report>* reports);

  OperationStatus RecordUploadAttempt(UploadReport* report,
                                      bool successful,
                                      const std::string& id);

  static bool ReadMetadata(const base::FilePath& path, Report* report);
  static bool WriteNewMetadata(const base::FilePath& path);
  static bool WriteMetadata(const base::FilePath& path, Report* report);

  base::FilePath base_dir_;
  Settings settings_;
  InitializationStateDcheck initialized_;

  DISALLOW_COPY_AND_ASSIGN(CrashReportDatabase);
};

}  // namespace crashpad

#endif  // CRASHPAD_CLIENT_CRASH_REPORT_DATABASE_H_
