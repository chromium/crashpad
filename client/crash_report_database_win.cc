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

#include <rpc.h>
#include <string.h>
#include <time.h>
#include <windows.h>

#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"

namespace crashpad {

namespace {

const wchar_t kDatabaseDirectoryName[] = L"Crashpad";

const wchar_t kReportsDirectory[] = L"reports";
const wchar_t kMetadataFileName[] = L"metadata";

const wchar_t kCrashReportFileExtension[] = L"dmp";

enum class ReportState : int {
  //! \brief Created and filled out by caller, owned by database.
  kPending,
  //! \brief In the process of uploading, owned by caller.
  kUploading,
  //! \brief Upload completed or skipped, owned by database.
  kCompleted,
};

using OperationStatus = CrashReportDatabase::OperationStatus;

//! \brief Ensures that the node at path is a directory, and creates it if it
//!     does not exist.
//!
//! \return If the path points to a file, rather than a directory, or the
//!     directory could not be created, returns `false`. Otherwise, returns
//!     `true`, indicating that path already was or now is a directory.
bool CreateOrEnsureDirectoryExists(const base::FilePath& path) {
  if (CreateDirectory(path.value().c_str(), nullptr)) {
    return true;
  } else if (GetLastError() == ERROR_ALREADY_EXISTS) {
    DWORD fileattr = GetFileAttributes(path.value().c_str());
    if (fileattr == INVALID_FILE_ATTRIBUTES) {
      PLOG(ERROR) << "GetFileAttributes";
      return false;
    }
    if ((fileattr & FILE_ATTRIBUTE_DIRECTORY) != 0)
      return true;
    LOG(ERROR) << "not a directory";
    return false;
  } else {
    PLOG(ERROR) << "CreateDirectory";
    return false;
  }
}

//! \brief A private extension of the Report class that includes additional data
//!     that's stored on disk in the metadata file.
struct ReportDisk : public CrashReportDatabase::Report {
  //! \brief The current state of the report.
  ReportState state;
};

//! \brief A private extension of the NewReport class to hold the UUID during
//!     initial write. We don't store metadata in dump's file attributes, and
//!     use the UUID to identify the dump on write completion.
struct NewReportDisk : public CrashReportDatabase::NewReport {
  //! \brief The UUID for this crash report.
  UUID uuid;
};

//! \brief Manages the metadata for the set of reports, handling serialization
//!     to disk, and queries. Instances of this class should be created by using
//!     CrashReportDatabaseWin::AcquireMetadata().
class Metadata {
 public:
  //! \brief Writes any changes if necessary, unlocks and closes the file
  //!     handle.
  ~Metadata();

  //! \brief Adds a new report to the set.
  //!
  //! \param[in] new_report_disk The record to add. The #state field must be set
  //!     to kPending.
  void AddNewRecord(const ReportDisk& new_report_disk);

  //! \brief Finds all reports in a given state. The \a reports vector is only
  //!     valid when CrashReportDatabase::kNoError is returned.
  //!
  //! \param[in] desired_state The state to match.
  //! \param[out] reports Matching reports, must be empty on entry.
  OperationStatus FindReports(
      ReportState desired_state,
      std::vector<const CrashReportDatabase::Report>* reports);

  //! \brief Finds the report matching the given UUID.
  //!
  //! The returned report is only valid if CrashReportDatabase::kNoError is
  //! returned.
  //!
  //! \param[in] uuid The report identifier.
  //! \param[out] report_disk The found report, valid only if
  //!     CrashReportDatabase::kNoError is returned. Ownership is not
  //!     transferred to the caller, and the report may not be modified.
  OperationStatus FindSingleReport(const UUID& uuid,
                                   const ReportDisk** report_disk);

  //! \brief Finds a single report matching the given UUID and in the desired
  //!     state and calls the client-supplied mutator to modify the report.
  //!
  //! The mutator object must have an operator()(ReportDisk*) which makes the
  //! desired changes.
  //!
  //! \return #kNoError on success. #kReportNotFound if there was no report with
  //!     the specified UUID. #kBusyError if the report was not in the specified
  //!     state.
  template <class T>
  OperationStatus MutateSingleReport(const UUID& uuid,
                                     ReportState desired_state,
                                     const T& mutator);

 private:
  static scoped_ptr<Metadata> Create(const base::FilePath& metadata_file,
                                     const base::FilePath& report_dir);
  friend class CrashReportDatabaseWin;

  Metadata(FileHandle handle, const base::FilePath& report_dir);

  bool Rewind();

  void Read();
  void Write();

  //! \brief Confirms that the corresponding report actually exists on disk
  //!     (that is, the dump file has not been removed), that the report is in
  //!     the given state.
  static OperationStatus VerifyReport(const ReportDisk& report_disk,
                                      ReportState desired_state);
  //! \brief Confirms that the corresponding report actually exists on disk
  //!     (that is, the dump file has not been removed).
  static OperationStatus VerifyReportAnyState(const ReportDisk& report_disk);

  ScopedFileHandle handle_;
  const base::FilePath report_dir_;
  bool dirty_;  //! \brief Is a Write() required on destruction?
  std::vector<ReportDisk> reports_;

  DISALLOW_COPY_AND_ASSIGN(Metadata);
};

Metadata::Metadata(FileHandle handle, const base::FilePath& report_dir)
    : handle_(handle), report_dir_(report_dir), dirty_(false), reports_() {
}

Metadata::~Metadata() {
  if (dirty_)
    Write();
  // Not actually async, UnlockFileEx requires the Offset fields.
  OVERLAPPED overlapped = {0};
  if (!UnlockFileEx(handle_.get(), 0, MAXDWORD, MAXDWORD, &overlapped))
    PLOG(ERROR) << "UnlockFileEx";
}

// The format of the metadata file is a MetadataFileHeader, followed by a
// number of fixed size records of MetadataFileReportRecord, followed by a
// string table in UTF8 format, where each string is \0 terminated.

#pragma pack(push, 1)

struct MetadataFileHeader {
  uint32_t magic;
  uint32_t version;
  uint32_t num_records;
  uint32_t padding;
};

struct MetadataFileReportRecord {
  UUID uuid;  // UUID is a 16 byte, standard layout structure.
  uint32_t file_path_index;  // Index into string table. File name is relative
                             // to the reports directory when on disk.
  uint32_t id_index;  // Index into string table.
  int64_t creation_time;  // Holds a time_t.
  int64_t last_upload_attempt_time;  // Holds a time_t.
  int32_t upload_attempts;
  int32_t state;  // A ReportState.
  uint8_t uploaded;  // Boolean, 0 or 1.
  uint8_t padding[7];
};

const uint32_t kMetadataFileHeaderMagic = 'CPAD';
const uint32_t kMetadataFileVersion = 1;

#pragma pack(pop)

// Reads from the current file position to EOF and returns as uint8_t[].
std::string ReadRestOfFileAsString(FileHandle file) {
  FileOffset read_from = LoggingSeekFile(file, 0, SEEK_CUR);
  FileOffset end = LoggingSeekFile(file, 0, SEEK_END);
  FileOffset original = LoggingSeekFile(file, read_from, SEEK_SET);
  if (read_from == -1 || end == -1 || original == -1)
    return std::string();
  DCHECK_EQ(read_from, original);
  DCHECK_GE(end, read_from);
  size_t data_length = static_cast<size_t>(end - read_from);
  std::string buffer(data_length, '\0');
  if (!LoggingReadFile(file, &buffer[0], data_length))
    return std::string();
  return buffer;
}

uint32_t AddStringToTable(std::string* string_table, const std::string& str) {
  uint32_t offset = base::checked_cast<uint32_t>(string_table->size());
  *string_table += str;
  *string_table += '\0';
  return offset;
}

uint32_t AddStringToTable(std::string* string_table, const std::wstring& str) {
  return AddStringToTable(string_table, base::UTF16ToUTF8(str));
}

// static
scoped_ptr<Metadata> Metadata::Create(const base::FilePath& metadata_file,
                                      const base::FilePath& report_dir) {
  // It is important that dwShareMode be non-zero so that concurrent access to
  // this file results in a successful open. This allows us to get to LockFileEx
  // which then blocks to guard access.
  FileHandle handle = CreateFile(metadata_file.value().c_str(),
                                 GENERIC_READ | GENERIC_WRITE,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 nullptr,
                                 OPEN_ALWAYS,
                                 FILE_ATTRIBUTE_NORMAL,
                                 nullptr);
  if (handle == kInvalidFileHandle)
    return scoped_ptr<Metadata>();
  // Not actually async, LockFileEx requires the Offset fields.
  OVERLAPPED overlapped = {0};
  if (!LockFileEx(handle,
                  LOCKFILE_EXCLUSIVE_LOCK,
                  0,
                  MAXDWORD,
                  MAXDWORD,
                  &overlapped)) {
    PLOG(ERROR) << "LockFileEx";
    return scoped_ptr<Metadata>();
  }

  scoped_ptr<Metadata> metadata(new Metadata(handle, report_dir));
  // If Read() fails, for whatever reason (corruption, etc.) metadata will not
  // have been modified and will be in a clean empty state. We continue on and
  // return an empty database to hopefully recover. This means that existing
  // crash reports have been orphaned.
  metadata->Read();
  return metadata;
}

bool Metadata::Rewind() {
  FileOffset result = LoggingSeekFile(handle_.get(), 0, SEEK_SET);
  DCHECK_EQ(result, 0);
  return result == 0;
}

void Metadata::Read() {
  FileOffset length = LoggingSeekFile(handle_.get(), 0, SEEK_END);
  if (length <= 0)  // Failed, or empty: Abort.
    return;
  if (!Rewind()) {
    LOG(ERROR) << "failed to rewind to read";
    return;
  }

  MetadataFileHeader header;
  if (!LoggingReadFile(handle_.get(), &header, sizeof(header))) {
    LOG(ERROR) << "failed to read header";
    return;
  }
  if (header.magic != kMetadataFileHeaderMagic ||
      header.version != kMetadataFileVersion) {
    LOG(ERROR) << "unexpected header";
    return;
  }

  auto records_size = base::CheckedNumeric<uint32_t>(header.num_records) *
                      sizeof(MetadataFileReportRecord);
  if (!records_size.IsValid()) {
    LOG(ERROR) << "record size out of range";
    return;
  }

  scoped_ptr<MetadataFileReportRecord[]> records(
      new MetadataFileReportRecord[header.num_records]);
  if (!LoggingReadFile(
          handle_.get(), records.get(), records_size.ValueOrDie())) {
    LOG(ERROR) << "failed to read records";
    return;
  }

  std::string string_table = ReadRestOfFileAsString(handle_.get());
  if (string_table.empty() || string_table.back() != '\0') {
    LOG(ERROR) << "bad string table";
    return;
  }
  for (uint32_t i = 0; i < header.num_records; ++i) {
    ReportDisk r;
    const MetadataFileReportRecord* record = &records[i];
    r.uuid = record->uuid;
    if (record->file_path_index >= string_table.size() ||
        record->id_index >= string_table.size()) {
      reports_.clear();
      LOG(ERROR) << "invalid string table index";
      return;
    }
    r.file_path = report_dir_.Append(
        base::UTF8ToUTF16(&string_table[record->file_path_index]));
    r.id = &string_table[record->id_index];
    r.creation_time = record->creation_time;
    r.uploaded = record->uploaded;
    r.last_upload_attempt_time = record->last_upload_attempt_time;
    r.upload_attempts = record->upload_attempts;
    r.state = static_cast<ReportState>(record->state);
    reports_.push_back(r);
  }
}

void Metadata::Write() {
  if (!Rewind()) {
    LOG(ERROR) << "failed to rewind to write";
    return;
  }

  // Truncate to ensure that a partial write doesn't cause a mix of old and new
  // data causing an incorrect interpretation on read.
  if (!SetEndOfFile(handle_.get())) {
    PLOG(ERROR) << "failed to truncate";
    return;
  }

  size_t num_records = reports_.size();

  // Fill and write out the header.
  MetadataFileHeader header = {0};
  header.magic = kMetadataFileHeaderMagic;
  header.version = kMetadataFileVersion;
  header.num_records = base::checked_cast<uint32_t>(num_records);
  if (!LoggingWriteFile(handle_.get(), &header, sizeof(header))) {
    LOG(ERROR) << "failed to write header";
    return;
  }

  // Build the records and string table we're going to write.
  std::string string_table;
  scoped_ptr<MetadataFileReportRecord[]> records(
      new MetadataFileReportRecord[num_records]);
  memset(records.get(), 0, sizeof(MetadataFileReportRecord) * num_records);
  for (size_t i = 0; i < num_records; ++i) {
    const ReportDisk& report = reports_[i];
    MetadataFileReportRecord& record = records[i];
    record.uuid = report.uuid;
    const base::FilePath& path = report.file_path;
    if (path.DirName() != report_dir_) {
      LOG(ERROR) << path.value().c_str() << " expected to start with "
                 << report_dir_.value().c_str();
      return;
    }
    record.file_path_index =
        AddStringToTable(&string_table, path.BaseName().value().c_str());
    record.id_index = AddStringToTable(&string_table, report.id);
    record.creation_time = report.creation_time;
    record.uploaded = report.uploaded;
    record.last_upload_attempt_time = report.last_upload_attempt_time;
    record.upload_attempts = report.upload_attempts;
    record.state = static_cast<uint32_t>(report.state);
  }

  if (!LoggingWriteFile(handle_.get(),
                        records.get(),
                        num_records * sizeof(MetadataFileReportRecord))) {
    LOG(ERROR) << "failed to write records";
    return;
  }
  if (!LoggingWriteFile(
          handle_.get(), string_table.c_str(), string_table.size())) {
    LOG(ERROR) << "failed to write string table";
    return;
  }
}

void Metadata::AddNewRecord(const ReportDisk& new_report_disk) {
  DCHECK(new_report_disk.state == ReportState::kPending);
  reports_.push_back(new_report_disk);
  dirty_ = true;
}

OperationStatus Metadata::FindReports(
    ReportState desired_state,
    std::vector<const CrashReportDatabase::Report>* reports) {
  DCHECK(reports->empty());
  for (const auto& report : reports_) {
    if (report.state == desired_state) {
      if (VerifyReport(report, desired_state) != CrashReportDatabase::kNoError)
        continue;
      reports->push_back(report);
    }
  }
  return CrashReportDatabase::kNoError;
}

OperationStatus Metadata::FindSingleReport(const UUID& uuid,
                                           const ReportDisk** out_report) {
  for (size_t i = 0; i < reports_.size(); ++i) {
    if (reports_[i].uuid == uuid) {
      OperationStatus os = VerifyReportAnyState(reports_[i]);
      if (os != CrashReportDatabase::kNoError)
        return os;
      *out_report = &reports_[i];
      return CrashReportDatabase::kNoError;
    }
  }
  return CrashReportDatabase::kReportNotFound;
}

template <class T>
OperationStatus Metadata::MutateSingleReport(
    const UUID& uuid,
    ReportState desired_state,
    const T& mutator) {
  for (size_t i = 0; i < reports_.size(); ++i) {
    if (reports_[i].uuid == uuid) {
      OperationStatus os = VerifyReport(reports_[i], desired_state);
      if (os != CrashReportDatabase::kNoError)
        return os;
      mutator(&reports_[i]);
      dirty_ = true;
      return CrashReportDatabase::kNoError;
    }
  }
  return CrashReportDatabase::kReportNotFound;
}

// static
OperationStatus Metadata::VerifyReportAnyState(const ReportDisk& report_disk) {
  DWORD fileattr = GetFileAttributes(report_disk.file_path.value().c_str());
  if (fileattr == INVALID_FILE_ATTRIBUTES)
    return CrashReportDatabase::kReportNotFound;
  if ((fileattr & FILE_ATTRIBUTE_DIRECTORY) != 0)
    return CrashReportDatabase::kFileSystemError;
  return CrashReportDatabase::kNoError;
}

// static
OperationStatus Metadata::VerifyReport(const ReportDisk& report_disk,
                                       ReportState desired_state) {
  if (report_disk.state != desired_state)
    return CrashReportDatabase::kBusyError;
  return VerifyReportAnyState(report_disk);
}

class CrashReportDatabaseWin : public CrashReportDatabase {
 public:
  explicit CrashReportDatabaseWin(const base::FilePath& path);
  ~CrashReportDatabaseWin() override;

  bool Initialize();

  // CrashReportDatabase:
  OperationStatus PrepareNewCrashReport(NewReport** report) override;
  OperationStatus FinishedWritingCrashReport(NewReport* report,
                                             UUID* uuid) override;
  OperationStatus ErrorWritingCrashReport(NewReport* report) override;
  OperationStatus LookUpCrashReport(const UUID& uuid, Report* report) override;
  OperationStatus GetPendingReports(
      std::vector<const Report>* reports) override;
  OperationStatus GetCompletedReports(
      std::vector<const Report>* reports) override;
  OperationStatus GetReportForUploading(const UUID& uuid,
                                        const Report** report) override;
  OperationStatus RecordUploadAttempt(const Report* report,
                                      bool successful,
                                      const std::string& id) override;
  OperationStatus SkipReportUpload(const UUID& uuid) override;

 private:
  scoped_ptr<Metadata> AcquireMetadata();

  base::FilePath base_dir_;

  DISALLOW_COPY_AND_ASSIGN(CrashReportDatabaseWin);
};

CrashReportDatabaseWin::CrashReportDatabaseWin(const base::FilePath& path)
    : CrashReportDatabase(), base_dir_(path) {
}

CrashReportDatabaseWin::~CrashReportDatabaseWin() {
}

bool CrashReportDatabaseWin::Initialize() {
  // Check if the database already exists.
  if (!CreateOrEnsureDirectoryExists(base_dir_))
    return false;

  // Create our reports subdirectory.
  if (!CreateOrEnsureDirectoryExists(base_dir_.Append(kReportsDirectory)))
    return false;

  // TODO(scottmg): When are completed reports pruned from disk? Delete here or
  // maybe on AcquireMetadata().

  return true;
}

OperationStatus CrashReportDatabaseWin::PrepareNewCrashReport(
    NewReport** out_report) {
  scoped_ptr<NewReportDisk> report(new NewReportDisk());

  ::UUID system_uuid;
  if (UuidCreate(&system_uuid) != RPC_S_OK) {
    return kFileSystemError;
  }
  static_assert(sizeof(system_uuid) == 16, "unexpected system uuid size");
  static_assert(offsetof(::UUID, Data1) == 0, "unexpected uuid layout");
  UUID uuid(reinterpret_cast<const uint8_t*>(&system_uuid.Data1));

  report->uuid = uuid;
  report->path =
      base_dir_.Append(kReportsDirectory)
          .Append(uuid.ToWideString() + L"." + kCrashReportFileExtension);
  report->handle = LoggingOpenFileForWrite(
      report->path, FileWriteMode::kCreateOrFail, FilePermissions::kOwnerOnly);
  if (report->handle == INVALID_HANDLE_VALUE)
    return kFileSystemError;

  *out_report = report.release();
  return kNoError;
}

OperationStatus CrashReportDatabaseWin::FinishedWritingCrashReport(
    NewReport* report,
    UUID* uuid) {
  // Take ownership of the report, and cast to our private version with UUID.
  scoped_ptr<NewReportDisk> scoped_report(static_cast<NewReportDisk*>(report));
  // Take ownership of the file handle.
  ScopedFileHandle handle(report->handle);

  scoped_ptr<Metadata> metadata(AcquireMetadata());
  if (!metadata)
    return kDatabaseError;
  ReportDisk report_disk;
  report_disk.uuid = scoped_report->uuid;
  report_disk.file_path = scoped_report->path;
  report_disk.creation_time = time(nullptr);
  report_disk.state = ReportState::kPending;
  metadata->AddNewRecord(report_disk);
  *uuid = report_disk.uuid;
  return kNoError;
}

OperationStatus CrashReportDatabaseWin::ErrorWritingCrashReport(
    NewReport* report) {
  // Take ownership of the report, and cast to our private version with UUID.
  scoped_ptr<NewReportDisk> scoped_report(static_cast<NewReportDisk*>(report));

  // Close the outstanding handle.
  LoggingCloseFile(report->handle);

  // We failed to write, so remove the dump file. There's no entry in the
  // metadata table yet.
  if (!DeleteFile(scoped_report->path.value().c_str())) {
    PLOG(ERROR) << "DeleteFile " << scoped_report->path.value().c_str();
    return CrashReportDatabase::kFileSystemError;
  }

  return kNoError;
}

OperationStatus CrashReportDatabaseWin::LookUpCrashReport(const UUID& uuid,
                                                          Report* report) {
  scoped_ptr<Metadata> metadata(AcquireMetadata());
  if (!metadata)
    return kDatabaseError;
  // Find and return a copy of the matching report.
  const ReportDisk* report_disk;
  OperationStatus os = metadata->FindSingleReport(uuid, &report_disk);
  if (os != kNoError)
    return os;
  *report = *report_disk;
  return kNoError;
}

OperationStatus CrashReportDatabaseWin::GetPendingReports(
    std::vector<const Report>* reports) {
  scoped_ptr<Metadata> metadata(AcquireMetadata());
  if (!metadata)
    return kDatabaseError;
  return metadata->FindReports(ReportState::kPending, reports);
}

OperationStatus CrashReportDatabaseWin::GetCompletedReports(
    std::vector<const Report>* reports) {
  scoped_ptr<Metadata> metadata(AcquireMetadata());
  if (!metadata)
    return kDatabaseError;
  return metadata->FindReports(ReportState::kCompleted, reports);
}

OperationStatus CrashReportDatabaseWin::GetReportForUploading(
    const UUID& uuid,
    const Report** report) {
  scoped_ptr<Metadata> metadata(AcquireMetadata());
  if (!metadata)
    return kDatabaseError;
  // TODO(scottmg): After returning this report to the client, there is no way
  // to reap this report if the uploader fails to call RecordUploadAttempt() or
  // SkipReportUpload() (if it crashed or was otherwise buggy). To resolve this,
  // one possibility would be to change the interface to be FileHandle based, so
  // that instead of giving the file_path back to the client and changing state
  // to kUploading, we return an exclusive access handle, and use that as the
  // signal that the upload is pending, rather than an update to state in the
  // metadata. Alternatively, there could be a "garbage collection" at startup
  // where any reports that are orphaned in the kUploading state are either
  // reset to kPending to retry, or discarded.
  return metadata->MutateSingleReport(
      uuid, ReportState::kPending, [report](ReportDisk* report_disk) {
        report_disk->state = ReportState::kUploading;
        // Create a copy for passing back to client. This will be freed in
        // RecordUploadAttempt.
        *report = new Report(*report_disk);
      });
}

OperationStatus CrashReportDatabaseWin::RecordUploadAttempt(
    const Report* report,
    bool successful,
    const std::string& id) {
  // Take ownership, allocated in GetReportForUploading.
  scoped_ptr<const Report> upload_report(report);
  scoped_ptr<Metadata> metadata(AcquireMetadata());
  if (!metadata)
    return kDatabaseError;
  return metadata->MutateSingleReport(
      report->uuid,
      ReportState::kUploading,
      [successful, id](ReportDisk* report_disk) {
        report_disk->uploaded = successful;
        report_disk->id = id;
        report_disk->last_upload_attempt_time = time(nullptr);
        report_disk->upload_attempts++;
        report_disk->state =
            successful ? ReportState::kCompleted : ReportState::kPending;
      });
}

OperationStatus CrashReportDatabaseWin::SkipReportUpload(const UUID& uuid) {
  scoped_ptr<Metadata> metadata(AcquireMetadata());
  if (!metadata)
    return kDatabaseError;
  return metadata->MutateSingleReport(
      uuid, ReportState::kPending, [](ReportDisk* report_disk) {
        report_disk->state = ReportState::kCompleted;
      });
}

scoped_ptr<Metadata> CrashReportDatabaseWin::AcquireMetadata() {
  base::FilePath metadata_file = base_dir_.Append(kMetadataFileName);
  return Metadata::Create(metadata_file, base_dir_.Append(kReportsDirectory));
}

}  // namespace

// static
scoped_ptr<CrashReportDatabase> CrashReportDatabase::Initialize(
    const base::FilePath& path) {
  scoped_ptr<CrashReportDatabaseWin> database_win(
      new CrashReportDatabaseWin(path.Append(kDatabaseDirectoryName)));
  if (!database_win->Initialize())
    database_win.reset();

  return scoped_ptr<CrashReportDatabase>(database_win.release());
}

}  // namespace crashpad
