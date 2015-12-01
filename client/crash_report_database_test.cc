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

#include "build/build_config.h"
#include "client/settings.h"
#include "gtest/gtest.h"
#include "test/errors.h"
#include "test/file.h"
#include "test/scoped_temp_dir.h"
#include "util/file/file_io.h"

namespace crashpad {
namespace test {
namespace {

class CrashReportDatabaseTest : public testing::Test {
 public:
  CrashReportDatabaseTest() {
  }

 protected:
  // testing::Test:
  void SetUp() override {
    db_ = CrashReportDatabase::Initialize(path());
    ASSERT_TRUE(db_);
  }

  void ResetDatabase() {
    db_.reset();
  }

  CrashReportDatabase* db() { return db_.get(); }
  base::FilePath path() const {
    return temp_dir_.path().Append(FILE_PATH_LITERAL("crashpad_test_database"));
  }

  void CreateCrashReport(CrashReportDatabase::Report* report) {
    CrashReportDatabase::NewReport* new_report = nullptr;
    ASSERT_EQ(CrashReportDatabase::kNoError,
              db_->PrepareNewCrashReport(&new_report));
    const char kTest[] = "test";
    ASSERT_TRUE(LoggingWriteFile(new_report->handle, kTest, sizeof(kTest)));

    UUID uuid;
    EXPECT_EQ(CrashReportDatabase::kNoError,
              db_->FinishedWritingCrashReport(new_report, &uuid));

    EXPECT_EQ(CrashReportDatabase::kNoError,
              db_->LookUpCrashReport(uuid, report));
    ExpectPreparedCrashReport(*report);
    ASSERT_TRUE(FileExists(report->file_path));
  }

  void UploadReport(const UUID& uuid, bool successful, const std::string& id) {
    Settings* const settings = db_->GetSettings();
    ASSERT_TRUE(settings);
    time_t times[2];
    ASSERT_TRUE(settings->GetLastUploadAttemptTime(&times[0]));

    const CrashReportDatabase::Report* report = nullptr;
    ASSERT_EQ(CrashReportDatabase::kNoError,
              db_->GetReportForUploading(uuid, &report));
    EXPECT_NE(UUID(), report->uuid);
    EXPECT_FALSE(report->file_path.empty());
    EXPECT_TRUE(FileExists(report->file_path)) << report->file_path.value();
    EXPECT_GT(report->creation_time, 0);
    EXPECT_EQ(CrashReportDatabase::kNoError,
              db_->RecordUploadAttempt(report, successful, id));

    ASSERT_TRUE(settings->GetLastUploadAttemptTime(&times[1]));
    EXPECT_NE(times[1], 0);
    EXPECT_GE(times[1], times[0]);
  }

  void ExpectPreparedCrashReport(const CrashReportDatabase::Report& report) {
    EXPECT_NE(UUID(), report.uuid);
    EXPECT_FALSE(report.file_path.empty());
    EXPECT_TRUE(FileExists(report.file_path)) << report.file_path.value();
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

  DISALLOW_COPY_AND_ASSIGN(CrashReportDatabaseTest);
};

TEST_F(CrashReportDatabaseTest, Initialize) {
  // Initialize the database for the first time, creating it.
  ASSERT_TRUE(db());

  Settings* settings = db()->GetSettings();

  UUID client_ids[3];
  ASSERT_TRUE(settings->GetClientID(&client_ids[0]));
  EXPECT_NE(client_ids[0], UUID());

  time_t last_upload_attempt_time;
  ASSERT_TRUE(settings->GetLastUploadAttemptTime(&last_upload_attempt_time));
  EXPECT_EQ(0, last_upload_attempt_time);

  // Close and reopen the database at the same path.
  ResetDatabase();
  EXPECT_FALSE(db());
  auto db = CrashReportDatabase::InitializeWithoutCreating(path());
  ASSERT_TRUE(db);

  settings = db->GetSettings();

  ASSERT_TRUE(settings->GetClientID(&client_ids[1]));
  EXPECT_EQ(client_ids[0], client_ids[1]);

  ASSERT_TRUE(settings->GetLastUploadAttemptTime(&last_upload_attempt_time));
  EXPECT_EQ(0, last_upload_attempt_time);

  // Check that the database can also be opened by the method that is permitted
  // to create it.
  db = CrashReportDatabase::Initialize(path());
  ASSERT_TRUE(db);

  settings = db->GetSettings();

  ASSERT_TRUE(settings->GetClientID(&client_ids[2]));
  EXPECT_EQ(client_ids[0], client_ids[2]);

  ASSERT_TRUE(settings->GetLastUploadAttemptTime(&last_upload_attempt_time));
  EXPECT_EQ(0, last_upload_attempt_time);

  std::vector<CrashReportDatabase::Report> reports;
  EXPECT_EQ(CrashReportDatabase::kNoError, db->GetPendingReports(&reports));
  EXPECT_TRUE(reports.empty());
  reports.clear();
  EXPECT_EQ(CrashReportDatabase::kNoError, db->GetCompletedReports(&reports));
  EXPECT_TRUE(reports.empty());

  // InitializeWithoutCreating() shouldn’t create a nonexistent database.
  base::FilePath non_database_path =
      path().DirName().Append(FILE_PATH_LITERAL("not_a_database"));
  db = CrashReportDatabase::InitializeWithoutCreating(non_database_path);
  EXPECT_FALSE(db);
}

TEST_F(CrashReportDatabaseTest, NewCrashReport) {
  CrashReportDatabase::NewReport* new_report;
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->PrepareNewCrashReport(&new_report));
  UUID expect_uuid = new_report->uuid;
  EXPECT_TRUE(FileExists(new_report->path)) << new_report->path.value();
  UUID uuid;
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->FinishedWritingCrashReport(new_report, &uuid));
  EXPECT_EQ(expect_uuid, uuid);

  CrashReportDatabase::Report report;
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->LookUpCrashReport(uuid, &report));
  ExpectPreparedCrashReport(report);

  std::vector<CrashReportDatabase::Report> reports;
  EXPECT_EQ(CrashReportDatabase::kNoError, db()->GetPendingReports(&reports));
  ASSERT_EQ(1u, reports.size());
  EXPECT_EQ(report.uuid, reports[0].uuid);

  reports.clear();
  EXPECT_EQ(CrashReportDatabase::kNoError, db()->GetCompletedReports(&reports));
  EXPECT_TRUE(reports.empty());
}

TEST_F(CrashReportDatabaseTest, ErrorWritingCrashReport) {
  CrashReportDatabase::NewReport* new_report = nullptr;
  ASSERT_EQ(CrashReportDatabase::kNoError,
            db()->PrepareNewCrashReport(&new_report));
  base::FilePath new_report_path = new_report->path;
  EXPECT_TRUE(FileExists(new_report_path)) << new_report_path.value();
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->ErrorWritingCrashReport(new_report));
  EXPECT_FALSE(FileExists(new_report_path)) << new_report_path.value();
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
    EXPECT_EQ(std::string(), report.id);
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
  UploadReport(reports[1].uuid, false, std::string());
  UploadReport(reports[2].uuid, true, "abc123");

  std::vector<CrashReportDatabase::Report> query(3);
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->LookUpCrashReport(reports[0].uuid, &query[0]));
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->LookUpCrashReport(reports[1].uuid, &query[1]));
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->LookUpCrashReport(reports[2].uuid, &query[2]));

  EXPECT_EQ(std::string(), query[0].id);
  EXPECT_EQ(std::string(), query[1].id);
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
  UploadReport(reports[1].uuid, false, std::string());

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

  std::vector<CrashReportDatabase::Report> pending;
  EXPECT_EQ(CrashReportDatabase::kNoError, db()->GetPendingReports(&pending));

  std::vector<CrashReportDatabase::Report> completed;
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->GetCompletedReports(&completed));

  EXPECT_EQ(reports.size(), pending.size());
  EXPECT_EQ(0u, completed.size());

  // Upload one report successfully.
  UploadReport(report_1_uuid, true, "report1");

  pending.clear();
  EXPECT_EQ(CrashReportDatabase::kNoError, db()->GetPendingReports(&pending));
  completed.clear();
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->GetCompletedReports(&completed));

  EXPECT_EQ(4u, pending.size());
  ASSERT_EQ(1u, completed.size());

  for (const auto& report : pending) {
    EXPECT_NE(report_1_uuid, report.uuid);
    EXPECT_FALSE(report.file_path.empty());
  }
  EXPECT_EQ(report_1_uuid, completed[0].uuid);
  EXPECT_EQ("report1", completed[0].id);
  EXPECT_EQ(true, completed[0].uploaded);
  EXPECT_GT(completed[0].last_upload_attempt_time, 0);
  EXPECT_EQ(1, completed[0].upload_attempts);

  // Fail to upload one report.
  UploadReport(report_2_uuid, false, std::string());

  pending.clear();
  EXPECT_EQ(CrashReportDatabase::kNoError, db()->GetPendingReports(&pending));
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
    EXPECT_FALSE(report.file_path.empty());
  }

  // Upload a second report.
  UploadReport(report_4_uuid, true, "report_4");

  pending.clear();
  EXPECT_EQ(CrashReportDatabase::kNoError, db()->GetPendingReports(&pending));
  completed.clear();
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->GetCompletedReports(&completed));

  EXPECT_EQ(3u, pending.size());
  ASSERT_EQ(2u, completed.size());

  // Succeed the failed report.
  UploadReport(report_2_uuid, true, "report 2");

  pending.clear();
  EXPECT_EQ(CrashReportDatabase::kNoError, db()->GetPendingReports(&pending));
  completed.clear();
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->GetCompletedReports(&completed));

  EXPECT_EQ(2u, pending.size());
  ASSERT_EQ(3u, completed.size());

  for (const auto& report : pending) {
    EXPECT_TRUE(report.uuid == report_0_uuid || report.uuid == report_3_uuid);
    EXPECT_FALSE(report.file_path.empty());
  }

  // Skip upload for one report.
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->SkipReportUpload(report_3_uuid));

  pending.clear();
  EXPECT_EQ(CrashReportDatabase::kNoError, db()->GetPendingReports(&pending));
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
    EXPECT_FALSE(report.file_path.empty());
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
            db()->RecordUploadAttempt(upload_report, true, std::string()));
}

TEST_F(CrashReportDatabaseTest, MoveDatabase) {
  CrashReportDatabase::NewReport* new_report;
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->PrepareNewCrashReport(&new_report));
  EXPECT_TRUE(FileExists(new_report->path)) << new_report->path.value();
  UUID uuid;
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->FinishedWritingCrashReport(new_report, &uuid));

  RelocateDatabase();

  CrashReportDatabase::Report report;
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->LookUpCrashReport(uuid, &report));
  ExpectPreparedCrashReport(report);
  EXPECT_TRUE(FileExists(report.file_path)) << report.file_path.value();
}

TEST_F(CrashReportDatabaseTest, ReportRemoved) {
  CrashReportDatabase::NewReport* new_report;
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->PrepareNewCrashReport(&new_report));
  EXPECT_TRUE(FileExists(new_report->path)) << new_report->path.value();
  UUID uuid;
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->FinishedWritingCrashReport(new_report, &uuid));

  CrashReportDatabase::Report report;
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->LookUpCrashReport(uuid, &report));

#if defined(OS_WIN)
  EXPECT_EQ(0, _wunlink(report.file_path.value().c_str()));
#else
  EXPECT_EQ(0, unlink(report.file_path.value().c_str()))
      << ErrnoMessage("unlink");
#endif

  EXPECT_EQ(CrashReportDatabase::kReportNotFound,
            db()->LookUpCrashReport(uuid, &report));
}

TEST_F(CrashReportDatabaseTest, DeleteReport) {
  CrashReportDatabase::Report keep_pending;
  CrashReportDatabase::Report delete_pending;
  CrashReportDatabase::Report keep_completed;
  CrashReportDatabase::Report delete_completed;

  CreateCrashReport(&keep_pending);
  CreateCrashReport(&delete_pending);
  CreateCrashReport(&keep_completed);
  CreateCrashReport(&delete_completed);

  EXPECT_TRUE(FileExists(keep_pending.file_path));
  EXPECT_TRUE(FileExists(delete_pending.file_path));
  EXPECT_TRUE(FileExists(keep_completed.file_path));
  EXPECT_TRUE(FileExists(delete_completed.file_path));

  UploadReport(keep_completed.uuid, true, "1");
  UploadReport(delete_completed.uuid, true, "2");

  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->LookUpCrashReport(keep_completed.uuid, &keep_completed));
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->LookUpCrashReport(delete_completed.uuid, &delete_completed));

  EXPECT_TRUE(FileExists(keep_completed.file_path));
  EXPECT_TRUE(FileExists(delete_completed.file_path));

  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->DeleteReport(delete_pending.uuid));
  EXPECT_FALSE(FileExists(delete_pending.file_path));
  EXPECT_EQ(CrashReportDatabase::kReportNotFound,
            db()->LookUpCrashReport(delete_pending.uuid, &delete_pending));
  EXPECT_EQ(CrashReportDatabase::kReportNotFound,
            db()->DeleteReport(delete_pending.uuid));

  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->DeleteReport(delete_completed.uuid));
  EXPECT_FALSE(FileExists(delete_completed.file_path));
  EXPECT_EQ(CrashReportDatabase::kReportNotFound,
            db()->LookUpCrashReport(delete_completed.uuid, &delete_completed));
  EXPECT_EQ(CrashReportDatabase::kReportNotFound,
            db()->DeleteReport(delete_completed.uuid));

  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->LookUpCrashReport(keep_pending.uuid, &keep_pending));
  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->LookUpCrashReport(keep_completed.uuid, &keep_completed));
}

TEST_F(CrashReportDatabaseTest, DeleteReportEmptyingDatabase) {
  CrashReportDatabase::Report report;
  CreateCrashReport(&report);

  EXPECT_TRUE(FileExists(report.file_path));

  UploadReport(report.uuid, true, "1");

  EXPECT_EQ(CrashReportDatabase::kNoError,
            db()->LookUpCrashReport(report.uuid, &report));

  EXPECT_TRUE(FileExists(report.file_path));

  // This causes an empty database to be written, make sure this is handled.
  EXPECT_EQ(CrashReportDatabase::kNoError, db()->DeleteReport(report.uuid));
  EXPECT_FALSE(FileExists(report.file_path));
}

TEST_F(CrashReportDatabaseTest, ReadEmptyDatabase) {
  CrashReportDatabase::Report report;
  CreateCrashReport(&report);
  EXPECT_EQ(CrashReportDatabase::kNoError, db()->DeleteReport(report.uuid));

  // Deleting and the creating another report causes an empty database to be
  // loaded. Make sure this is handled.

  CrashReportDatabase::Report report2;
  CreateCrashReport(&report2);
}

}  // namespace
}  // namespace test
}  // namespace crashpad
