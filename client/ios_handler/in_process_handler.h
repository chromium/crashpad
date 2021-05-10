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

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "handler/mac/crash_report_exception_handler.h"
#include "snapshot/ios/process_snapshot_ios_intermediatedump.h"
#include "util/ios/ios_system_data_collector.h"
#include "util/misc/initialization_state_dcheck.h"

namespace crashpad {
namespace internal {

//! \brief Manage intermediate minidump generation, and own the crash report
//!     upload thread and database.
class InProcessHandler {
 public:
  InProcessHandler();
  ~InProcessHandler();

  //! \brief Initializes the in-process handler.
  //!
  //! This method must be called only once, and must be successfully called
  //! before any other method in this class may be called.
  //!
  //! \param[in] database The path to a Crashpad database.
  //! \param[in] url The URL of an upload server.
  //! \param[in] annotations Process annotations to set in each crash report.
  //! \return `true` if a handler to a pending intermediate dump could be
  //!     opened.
  bool Initialize(const base::FilePath& database,
                  const std::string& url,
                  const std::map<std::string, std::string>& annotations);

  //! \brief Generate an intermediate dump from a signal handler exception.
  //!
  //! \param[in] system_data An object containing various system data points.
  //! \param[in] siginfo A pointer to a `siginfo_t` object received by a signal
  //!     handler.
  //! \param[in] context A pointer to a `ucontext_t` object received by a
  //!     signal.
  void DumpExceptionFromSignal(const IOSSystemDataCollector& system_data,
                               siginfo_t* siginfo,
                               ucontext_t* context);

  //! \brief Generate an intermediate dump from a mach exception.
  //!
  //! \param[in] system_data An object containing various system data points.
  //! \param[in] behavior
  //! \param[in] thread
  //! \param[in] exception
  //! \param[in] code
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

  //! \brief Generate an intermediate dump from an uncaught NSException.
  //!
  //! When the ObjcExceptionPreprocessor does not detect an NSException as it is
  //! thrown, the last-chance uncaught exception handler passes a list of call
  //! stack frame addresses.  Record them in the intermediate dump so a minidump
  //! with a 'fake' call stack is generated.
  //!
  //! \param[in] system_data An object containing various system data points.
  //! \param[in] frames An array of call stack frame addresses.
  //! \param[in] num_frames The number of frames in |frames|.
  void DumpExceptionFromNSExceptionFrames(
      const IOSSystemDataCollector& system_data,
      const uint64_t* frames,
      const size_t num_frames);

  //! \brief Requests that the handler convert all intermediate dumps into
  //!     minidumps and trigger an upload if possible.
  //!
  //! \param[in] annotations Process annotations to set in each crash report.
  void ProcessIntermediateDumps(
      const std::map<std::string, std::string>& annotations);

  //! \brief Requests that the handler convert a specific intermediate dump into
  //!     a minidump and trigger an upload if possible.
  //!
  //! \param[in] path Path to the specific intermediate dump.
  //! \param[in] annotations Process annotations to set in each crash report.
  void ProcessIntermediateDump(
      const base::FilePath& path,
      const std::map<std::string, std::string>& annotations = {});

  //! \brief Requests that the handler begin in-process uploading of any
  //! pending reports.
  void StartProcesingPendingReports();

  //! \brief Returns the FilePath to the current intermediate dump handle. To be
  //!     used for processing a single intermediate dump by
  //!     ProcessIntermediateDump.
  //! \return The full path to the current intermediate dump handle.
  base::FilePath CurrentFile() { return current_file_; }

 private:
  //! \brief Helper to start and end intermediate reports.
  class ScopedReport {
   public:
    ScopedReport(InProcessHandler* handler,
                 const IOSSystemDataCollector& system_data,
                 const uint64_t* frames = nullptr,
                 const size_t num_frames = 0);
    ~ScopedReport();
    InProcessHandler* handler_;
    DISALLOW_COPY_AND_ASSIGN(ScopedReport);
  };

  void SaveSnapshot(ProcessSnapshotIOSIntermediatedump& process_snapshot);
  std::vector<base::FilePath> PendingFiles();
  bool OpenNewFile();

  bool upload_thread_started_ = false;
  std::map<std::string, std::string> annotations_;
  base::FilePath base_dir_;
  base::FilePath current_file_;
  std::unique_ptr<IOSIntermediatedumpWriter> writer_;
  std::unique_ptr<CrashReportUploadThread> upload_thread_;
  std::unique_ptr<CrashReportDatabase> database_;
  InitializationStateDcheck initialized_;

  DISALLOW_COPY_AND_ASSIGN(InProcessHandler);
};

}  // namespace internal
}  // namespace crashpad
