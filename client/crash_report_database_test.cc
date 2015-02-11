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

#include <sys/stat.h>

#include "gtest/gtest.h"
#include "util/file/file_io.h"
#include "util/test/scoped_temp_dir.h"

namespace crashpad {
namespace test {
namespace {

bool FileExistsAtPath(const base::FilePath& path) {
#if defined(OS_POSIX)
  struct stat st;
  return lstat(path.value().c_str(), &st) == 0;
#elif defined(OS_WIN)
  struct _stat st;
  return _wstat(path.value().c_str(), &st) == 0;
#else
#error "Not implemented"
#endif
}

void CreateFile(const base::FilePath& path) {
  FileHandle handle = LoggingOpenFileForWrite(path,
                                              FileWriteMode::kCreateOrFail,
                                              FilePermissions::kWorldReadable);
#if defined(OS_POSIX)
  ASSERT_GE(handle, 0);
#elif defined(OS_WIN)
  ASSERT_NE(handle, nullptr);
#endif
  ASSERT_TRUE(
      LoggingWriteFile(handle, path.value().c_str(), path.value().length()));
  ASSERT_TRUE(LoggingCloseFile(handle));
}

class CrashReportDatabaseTest : public testing::Test {
 protected:
  // testing::Test:
  void SetUp() override {
    db_ = CrashReportDatabase::Initialize(path());
    ASSERT_TRUE(db_.get());
  }

  void ResetDatabase() {
    db_.reset();
  }

  CrashReportDatabase* db() const { return db_.get(); }
  const base::FilePath& path() const { return temp_dir_.path(); }

  void CreateCrashReport(CrashReportDatabase::Report* report) {
    CrashReportDatabase::NewReport* new_report;
    EXPECT_EQ(CrashReportDatabase::kNoError,
              db_->PrepareNewCrashReport(&new_report));
    const char kTest[] = "test";
    ASSERT_TRUE(LoggingWriteFile(new_report->handle, kTest, sizeof(kTest)));

    UUID uuid;
    EXPECT_EQ(CrashReportDatabase::kNoError,
              db_->FinishedWritingCrashReport(new_report, &uuid));

    EXPECT_EQ(CrashReportDatabase::kNoError,
              db_->LookUpCrashReport(uuid, report));
    ExpectPreparedCrashReport(*report);
    ASSERT_TRUE(FileExistsAtPath(report->file_path));
  }

  void UploadReport(const UUID& uuid, bool successful, const std::string& id) {
    const CrashReportDatabase::Report* report = nullptr;
    EXPECT_EQ(CrashReportDatabase::kNoError,
              db_->GetReportForUploading(uuid, &report));
    EXPECT_TRUE(report);
    EXPECT_NE(UUID(), report->uuid);
    EXPECT_FALSE(report->file_path.empty());
    EXPECT_TRUE(FileExistsAtPath(report->file_path))
        << report->file_path.value();
    EXPECT_GT(report->creation_time, 0);
    EXPECT_EQ(CrashReportDatabase::kNoError,
              db_->RecordUploadAttempt(report, successful, id));
  }

  void ExpectPreparedCrashReport(const CrashReportDatabase::Report& report) {
    EXPECT_NE(UUID(), report.uuid);
    EXPECT_FALSE(report.file_path.empty());
    EXPECT_TRUE(FileExistsAtPath(report.file_path)) << report.file_path.value();
    EXPECT_TRUE(report.id.empty());
    EXPECT_GT(report.creation_time, 0);
    EXPECT_FALSE(report.uploaded);
    EXPECT_EQ(0, report.last_upload_attempt_time);
    EXPECT_EQ(0, report.upload_attempts);
  }

  void RelocateDatabase() {
    ResetDatabase();
    temp_dir_.Rename();
    SetUp();
  }

 private:
  ScopedTempDir temp_dir_;
  scoped_ptr<CrashReportDatabase> db_;
};

TEST_F(CrashReportDatabaseTest, Initialize) {
  // Initialize the database for the first time, creating it.
  EXPECT_TRUE(db());

  // Close and reopen the database at the same path.
  ResetDatabase();
  EXPECT_FALSE(db());
  auto db = CrashReportDatabase::Initialize(path());
  EXPECT_TRUE(db.get());

  std::vector<const CrashReportDatabase::Report> reports;
  EXPECT_EQ(CrashReportDatabase::kNoError, db->GetPendingReports(&reports));
  EXPECT_TRUE(reports.empty());
  EXPECT_EQ(CrashReportDatabase::kNoError, db->GetCompletedReports(&reports));
  EXPECT_TRUE(reports.empty());
}

TEST_F(CrashReportDatabaseTest, NewCrashReport) {
  CrashReportDatabase::NewReport* new_report;
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->PrepareNewCrashReport(&new_report));
  EXPECT_TRUE(FileExistsAtPath(new_report->path)) << new_report->path.value();
  UUID uuid;
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->FinishedWritingCrashReport(new_report, &uuid));

  CrashReportDatabase::Report report;
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->LookUpCrashReport(uuid, &report));
  ExpectPreparedCrashReport(report);

  std::vector<const CrashReportDatabase::Report> reports;
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->GetPendingReports(&reports));
  ASSERT_EQ(1u, reports.size());
  EXPECT_EQ(report.uuid, reports[0].uuid);

  reports.clear();
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->GetCompletedReports(&reports));
  EXPECT_TRUE(reports.empty());
}

TEST_F(CrashReportDatabaseTest, ErrorWritingCrashReport) {
  CrashReportDatabase::NewReport* new_report;
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->PrepareNewCrashReport(&new_report));
  base::FilePath new_report_path = new_report->path;
  EXPECT_TRUE(FileExistsAtPath(new_report_path)) << new_report_path.value();
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->ErrorWritingCrashReport(new_report));
  EXPECT_FALSE(FileExistsAtPath(new_report_path)) << new_report_path.value();
}

TEST_F(CrashReportDatabaseTest, LookUpCrashReport) {
  UUID uuid;

  {
    CrashReportDatabase::Report report;
    CreateCrashReport(&report);
    uuid = report.uuid;
  }

  {
    CrashReportDatabase::Report report;
    EXPECT_EQ(CrashReportDatabase::kNoError,
              db()->LookUpCrashReport(uuid, &report));
    EXPECT_EQ(uuid, report.uuid);
    EXPECT_NE(std::string::npos, report.file_path.value().find(path().value()));
    EXPECT_EQ("", report.id);
    EXPECT_FALSE(report.uploaded);
    EXPECT_EQ(0, report.last_upload_attempt_time);
    EXPECT_EQ(0, report.upload_attempts);
  }

  UploadReport(uuid, true, "test");

  {
    CrashReportDatabase::Report report;
    EXPECT_EQ(CrashReportDatabase::kNoError,
              db()->LookUpCrashReport(uuid, &report));
    EXPECT_EQ(uuid, report.uuid);
    EXPECT_NE(std::string::npos, report.file_path.value().find(path().value()));
    EXPECT_EQ("test", report.id);
    EXPECT_TRUE(report.uploaded);
    EXPECT_NE(0, report.last_upload_attempt_time);
    EXPECT_EQ(1, report.upload_attempts);
  }
}

TEST_F(CrashReportDatabaseTest, RecordUploadAttempt) {
  std::vector<CrashReportDatabase::Report> reports(3);
  CreateCrashReport(&reports[0]);
  CreateCrashReport(&reports[1]);
  CreateCrashReport(&reports[2]);

  // Record two attempts: one successful, one not.
  UploadReport(reports[1].uuid, false, "");
  UploadReport(reports[2].uuid, true, "abc123");

  std::vector<CrashReportDatabase::Report> query(3);

  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->LookUpCrashReport(reports[0].uuid, &query[0]));
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->LookUpCrashReport(reports[1].uuid, &query[1]));
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->LookUpCrashReport(reports[2].uuid, &query[2]));

  EXPECT_EQ("", query[0].id);
  EXPECT_EQ("", query[1].id);
  EXPECT_EQ("abc123", query[2].id);

  EXPECT_FALSE(query[0].uploaded);
  EXPECT_FALSE(query[1].uploaded);
  EXPECT_TRUE(query[2].uploaded);

  EXPECT_EQ(0, query[0].last_upload_attempt_time);
  EXPECT_NE(0, query[1].last_upload_attempt_time);
  EXPECT_NE(0, query[2].last_upload_attempt_time);

  EXPECT_EQ(0, query[0].upload_attempts);
  EXPECT_EQ(1, query[1].upload_attempts);
  EXPECT_EQ(1, query[2].upload_attempts);

  // Attempt to upload and fail again.
  UploadReport(reports[1].uuid, false, "");

  time_t report_2_upload_time = query[2].last_upload_attempt_time;

  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->LookUpCrashReport(reports[0].uuid, &query[0]));
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->LookUpCrashReport(reports[1].uuid, &query[1]));
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->LookUpCrashReport(reports[2].uuid, &query[2]));

  EXPECT_FALSE(query[0].uploaded);
  EXPECT_FALSE(query[1].uploaded);
  EXPECT_TRUE(query[2].uploaded);

  EXPECT_EQ(0, query[0].last_upload_attempt_time);
  EXPECT_GE(query[1].last_upload_attempt_time, report_2_upload_time);
  EXPECT_EQ(report_2_upload_time, query[2].last_upload_attempt_time);

  EXPECT_EQ(0, query[0].upload_attempts);
  EXPECT_EQ(2, query[1].upload_attempts);
  EXPECT_EQ(1, query[2].upload_attempts);

  // Third time's the charm: upload and succeed.
  UploadReport(reports[1].uuid, true, "666hahaha");

  time_t report_1_upload_time = query[1].last_upload_attempt_time;

  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->LookUpCrashReport(reports[0].uuid, &query[0]));
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->LookUpCrashReport(reports[1].uuid, &query[1]));
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->LookUpCrashReport(reports[2].uuid, &query[2]));

  EXPECT_FALSE(query[0].uploaded);
  EXPECT_TRUE(query[1].uploaded);
  EXPECT_TRUE(query[2].uploaded);

  EXPECT_EQ(0, query[0].last_upload_attempt_time);
  EXPECT_GE(query[1].last_upload_attempt_time, report_1_upload_time);
  EXPECT_EQ(report_2_upload_time, query[2].last_upload_attempt_time);

  EXPECT_EQ(0, query[0].upload_attempts);
  EXPECT_EQ(3, query[1].upload_attempts);
  EXPECT_EQ(1, query[2].upload_attempts);
}

// This test covers both query functions since they are related.
TEST_F(CrashReportDatabaseTest, GetCompletedAndNotUploadedReports) {
  std::vector<CrashReportDatabase::Report> reports(5);
  CreateCrashReport(&reports[0]);
  CreateCrashReport(&reports[1]);
  CreateCrashReport(&reports[2]);
  CreateCrashReport(&reports[3]);
  CreateCrashReport(&reports[4]);

  const UUID& report_0_uuid = reports[0].uuid;
  const UUID& report_1_uuid = reports[1].uuid;
  const UUID& report_2_uuid = reports[2].uuid;
  const UUID& report_3_uuid = reports[3].uuid;
  const UUID& report_4_uuid = reports[4].uuid;

  std::vector<const CrashReportDatabase::Report> pending;
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->GetPendingReports(&pending));

  std::vector<const CrashReportDatabase::Report> completed;
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->GetCompletedReports(&completed));

  EXPECT_EQ(reports.size(), pending.size());
  EXPECT_EQ(0u, completed.size());

  // Upload one report successfully.
  UploadReport(report_1_uuid, true, "report1");

  pending.clear();
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->GetPendingReports(&pending));
  completed.clear();
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->GetCompletedReports(&completed));

  EXPECT_EQ(4u, pending.size());
  ASSERT_EQ(1u, completed.size());

  for (const auto& report : pending)
    EXPECT_NE(report_1_uuid, report.uuid);
  EXPECT_EQ(report_1_uuid, completed[0].uuid);
  EXPECT_EQ("report1", completed[0].id);
  EXPECT_EQ(true, completed[0].uploaded);
  EXPECT_GT(completed[0].last_upload_attempt_time, 0);
  EXPECT_EQ(1, completed[0].upload_attempts);

  const CrashReportDatabase::Report completed_report_1 = completed[0];

  // Fail to upload one report.
  UploadReport(report_2_uuid, false, "");

  pending.clear();
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->GetPendingReports(&pending));
  completed.clear();
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->GetCompletedReports(&completed));

  EXPECT_EQ(4u, pending.size());
  ASSERT_EQ(1u, completed.size());

  for (const auto& report : pending) {
    if (report.upload_attempts != 0) {
      EXPECT_EQ(report_2_uuid, report.uuid);
      EXPECT_GT(report.last_upload_attempt_time, 0);
      EXPECT_FALSE(report.uploaded);
      EXPECT_TRUE(report.id.empty());
    }
  }

  // Upload a second report.
  UploadReport(report_4_uuid, true, "report_4");

  pending.clear();
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->GetPendingReports(&pending));
  completed.clear();
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->GetCompletedReports(&completed));

  EXPECT_EQ(3u, pending.size());
  ASSERT_EQ(2u, completed.size());

  // Succeed the failed report.
  UploadReport(report_2_uuid, true, "report 2");

  pending.clear();
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->GetPendingReports(&pending));
  completed.clear();
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->GetCompletedReports(&completed));

  EXPECT_EQ(2u, pending.size());
  ASSERT_EQ(3u, completed.size());

  for (const auto& report : pending) {
    EXPECT_TRUE(report.uuid == report_0_uuid ||
                report.uuid == report_3_uuid);
  }

  // Skip upload for one report.
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->SkipReportUpload(report_3_uuid));

  pending.clear();
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->GetPendingReports(&pending));
  completed.clear();
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->GetCompletedReports(&completed));

  ASSERT_EQ(1u, pending.size());
  ASSERT_EQ(4u, completed.size());

  EXPECT_EQ(report_0_uuid, pending[0].uuid);

  for (const auto& report : completed) {
    if (report.uuid == report_3_uuid) {
      EXPECT_FALSE(report.uploaded);
      EXPECT_EQ(0, report.upload_attempts);
      EXPECT_EQ(0, report.last_upload_attempt_time);
    } else {
      EXPECT_TRUE(report.uploaded);
      EXPECT_GT(report.upload_attempts, 0);
      EXPECT_GT(report.last_upload_attempt_time, 0);
    }
  }
}

TEST_F(CrashReportDatabaseTest, DuelingUploads) {
  CrashReportDatabase::Report report;
  CreateCrashReport(&report);

  const CrashReportDatabase::Report* upload_report;
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->GetReportForUploading(report.uuid, &upload_report));

  const CrashReportDatabase::Report* upload_report_2 = nullptr;
  EXPECT_EQ(CrashReportDatabase::kBusyError,
            db()->GetReportForUploading(report.uuid, &upload_report_2));
  EXPECT_FALSE(upload_report_2);

  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->RecordUploadAttempt(upload_report, true, ""));
}

TEST_F(CrashReportDatabaseTest, MoveDatabase) {
  CrashReportDatabase::NewReport* new_report;
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->PrepareNewCrashReport(&new_report));
  EXPECT_TRUE(FileExistsAtPath(new_report->path)) << new_report->path.value();
  UUID uuid;
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->FinishedWritingCrashReport(new_report, &uuid));

  RelocateDatabase();

  CrashReportDatabase::Report report;
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->LookUpCrashReport(uuid, &report));
  ExpectPreparedCrashReport(report);
  EXPECT_TRUE(FileExistsAtPath(report.file_path)) << report.file_path.value();
}

}  // namespace
}  // namespace test
}  // namespace crashpad
