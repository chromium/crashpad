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
#include "snapshot/ios/process_snapshot_ios.h"
#include "util/ios/ios_system_data_collector.h"
#include "util/misc/initialization_state_dcheck.h"

namespace crashpad {
namespace internal {

//! \brief Manage intermediate minidumps, crash report upload thread and
//! database.
class InProcessHandler {
 public:
  InProcessHandler() = default;

  //! \brief Set up the in process handler database and upload thread.
  //!
  //! \param[in] database The path to a Crashpad database. The handler will be
  //!     started with this path as its `--database` argument.
  //! \param[in] metrics_dir The path to an already existing directory where
  //!     metrics files can be stored. The handler will be started with this
  //!     path as its `--metrics-dir` argument.
  //! \param[in] url The URL of an upload server. The handler will be started
  //!     with this URL as its `--url` argument.
  //! \param[in] annotations Process annotations to set in each crash report.
  //!     The handler will be started with an `--annotation` argument for each
  //!     element in this map.
  void Initialize(const base::FilePath& database,
                  const base::FilePath& metrics_dir,
                  const std::string& url,
                  const std::map<std::string, std::string>& annotations);

  //! \brief Generate an intermediate minidump from a signal handler.
  //!
  //! \param[in] system_data A 'IOSSystemDataCollector' object containing system
  //!     level data from before a crash occured.
  //! \param[in] siginfo A pointer to a `siginfo_t` object received by a signal
  //!     handler.
  //! \param[in] context A pointer to a `ucontext_t` object received by a signal
  //!     handler.
  void DumpExceptionFromSignal(const IOSSystemDataCollector& system_data,
                               siginfo_t* siginfo,
                               ucontext_t* context);

  //! \brief Generate an intermediate minidump from a mach exception.
  //!
  //! \param[in] system_data A 'IOSSystemDataCollector' object containing system
  //!     level data from before a crash occured.
  //! \param[in] behavior The exception behavior, which dictates which function
  //!     will be called. It is an error to call this function with an invalid
  //!     value for \a behavior.
  //! \param[in] thread
  //! \param[in] exception
  //! \param[in] code If \a behavior indicates a behavior without
  //!     `MACH_EXCEPTION_CODES`, the elements of \a code will be truncated in
  //!     order to be passed to the appropriate exception handler.
  //! \param[in] code_count
  //! \param[in,out] flavor
  //! \param[in] old_state
  //! \param[in] old_state_count
  void DumpExceptionFromMachException(const IOSSystemDataCollector& system_data,
                                      exception_behavior_t behavior,
                                      thread_t thread,
                                      exception_type_t exception,
                                      const mach_exception_data_type_t* code,
                                      mach_msg_type_number_t code_count,
                                      thread_state_flavor_t flavor,
                                      ConstThreadState old_state,
                                      mach_msg_type_number_t old_state_count);

  //! \brief Load, convert and save intermediate minidumps into minidumps and
  //! save to the crash database.
  void ProcessPendingDumps();

 private:
  // TODO(justincohen): SaveSnapshot should take an intermediate dump instead.
  void SaveSnapshot(ProcessSnapshotIOS& process_snapshot);
  std::vector<base::FilePath> PendingFiles();

  bool OpenNewFile();
  std::map<std::string, std::string> annotations_;
  base::FilePath base_dir_;
  base::FilePath current_file_;
  CrashReportUploadThread* upload_thread_;
  std::unique_ptr<CrashReportDatabase> database_;
  InitializationStateDcheck initialized_;

  DISALLOW_COPY_AND_ASSIGN(InProcessHandler);
};

}  // namespace internal
}  // namespace crashpad
