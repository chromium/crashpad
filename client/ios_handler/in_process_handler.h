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

#include <vector>

#include "base/files/file_path.h"
#include "handler/mac/crash_report_exception_handler.h"
#include "snapshot/ios/process_snapshot_ios_intermediatedump.h"
#include "util/ios/ios_system_data_collector.h"
#include "util/misc/initialization_state_dcheck.h"

namespace crashpad {
namespace internal {

//! \brief Manage interim minidumps access and own access to the crash report
//! upload thread and database.
class InProcessHandler {
 public:
  InProcessHandler() = default;

  void Initialize(const base::FilePath& database,
                  const std::string& url,
                  const std::map<std::string, std::string>& annotations);

  void DumpExceptionFromSignal(const IOSSystemDataCollector& system_data,
                               siginfo_t* siginfo,
                               ucontext_t* context);

  void DumpExceptionFromMachException(const IOSSystemDataCollector& system_data,
                                      exception_behavior_t behavior,
                                      thread_t thread,
                                      exception_type_t exception,
                                      const mach_exception_data_type_t* code,
                                      mach_msg_type_number_t code_count,
                                      thread_state_flavor_t flavor,
                                      ConstThreadState old_state,
                                      mach_msg_type_number_t old_state_count);

  void ProcessPendingDumps();

 private:
  void SaveSnapshot(ProcessSnapshotIOSIntermediatedump& process_snapshot);
  std::vector<base::FilePath> PendingFiles();

  void OpenNewFile();
  void StartReport(const IOSSystemDataCollector& system_data);
  void EndReport();
  std::map<std::string, std::string> annotations_;
  int fd_;
  base::FilePath base_dir_;
  base::FilePath current_file_;
  CrashReportUploadThread* upload_thread_;
  std::unique_ptr<CrashReportDatabase> database_;
  InitializationStateDcheck initialized_;

  DISALLOW_COPY_AND_ASSIGN(InProcessHandler);
};

}  // namespace internal
}  // namespace crashpad
