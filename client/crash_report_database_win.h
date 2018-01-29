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

#include "base/macros.h"
#include "client/crash_report_database.h"

namespace crashpad {

class CrashReportDatabaseWin : public CrashReportDatabase {
 public:
  ~CrashReportDatabaseWin() override;

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
  int CleanDatabase(time_t ttl) override;

 protected:
  CrashReportDatabaseWin();

 private:
  OperationStatus RecordUploadAttempt(UploadReport* report,
                                      bool successful,
                                      const std::string& id) override;

  DISALLOW_COPY_AND_ASSIGN(CrashReportDatabaseWin);
};

}  // namespace crashpad
