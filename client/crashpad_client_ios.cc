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

#include "client/crashpad_client.h"

#include <unistd.h>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "client/client_argv_handling.h"
#include "client/crash_report_database.h"
#include "minidump/minidump_file_writer.h"
#include "snapshot/ios/process_snapshot_ios.h"
#include "util/ios/exception_processor.h"
#include "util/ios/ios_system_data_collector.h"
#include "util/posix/signals.h"

namespace crashpad {

namespace {

// A base class for Crashpad signal handler implementations.
class SignalHandler {
 public:
  // Returns the currently installed signal hander.
  static SignalHandler* Get() {
    static SignalHandler* instance = new SignalHandler();
    return instance;
  }

  bool Install(const std::set<int>* unhandled_signals) {
    return Signals::InstallCrashHandlers(
        HandleSignal, 0, &old_actions_, unhandled_signals);
  }

  void HandleCrash(int signo, siginfo_t* siginfo, void* context) {
    // TODO(justincohen): This is incomplete.
    ProcessSnapshotIOS process_snapshot;
    process_snapshot.Initialize(system_data);
    process_snapshot.SetException(siginfo,
                                  reinterpret_cast<ucontext_t*>(context));
    test_only_dump(process_snapshot);
  }

 private:
  SignalHandler() = default;

  // The base implementation for all signal handlers, suitable for calling
  // directly to simulate signal delivery.
  void HandleCrashAndReraiseSignal(int signo,
                                   siginfo_t* siginfo,
                                   void* context) {
    HandleCrash(signo, siginfo, context);
    // Always call system handler.
    Signals::RestoreHandlerAndReraiseSignalOnReturn(
        siginfo, old_actions_.ActionForSignal(signo));
  }

  // TODO(justincohen): For testing only!
  // To be removed before landing CL, but super useful in testing.
  void test_only_dump(ProcessSnapshotIOS& process_snapshot) {
    LOG(INFO) << "test_only_dump";
    char* tmpdir = getenv("TMPDIR");
    std::string dir;
    dir.assign(tmpdir);
    dir.append("org.chromium.crashpad.test");
    mkdtemp(&dir[0]);
    std::unique_ptr<CrashReportDatabase> database(
        CrashReportDatabase::Initialize(base::FilePath(dir)));
    std::unique_ptr<CrashReportDatabase::NewReport> new_report;
    CrashReportDatabase::OperationStatus database_status =
        database->PrepareNewCrashReport(&new_report);
    if (database_status != CrashReportDatabase::kNoError) {
      Metrics::ExceptionCaptureResult(
          Metrics::CaptureResult::kPrepareNewCrashReportFailed);
    }
    process_snapshot.SetReportID(new_report->ReportID());
    MinidumpFileWriter minidump;
    minidump.InitializeFromSnapshot(&process_snapshot);
    if (!minidump.WriteEverything(new_report->Writer())) {
      Metrics::ExceptionCaptureResult(
          Metrics::CaptureResult::kMinidumpWriteFailed);
    }
    UUID uuid;
    database_status =
        database->FinishedWritingCrashReport(std::move(new_report), &uuid);
    if (database_status != CrashReportDatabase::kNoError) {
      Metrics::ExceptionCaptureResult(
          Metrics::CaptureResult::kFinishedWritingCrashReportFailed);
    }
  }

  // The signal handler installed at OS-level.
  static void HandleSignal(int signo, siginfo_t* siginfo, void* context) {
    Get()->HandleCrashAndReraiseSignal(signo, siginfo, context);
  }

  Signals::OldActions old_actions_ = {};

  // Collect some system data before the signal handler is triggered.
  IOSSystemDataCollector system_data;

  DISALLOW_COPY_AND_ASSIGN(SignalHandler);
};

}  // namespace

CrashpadClient::CrashpadClient() {}

CrashpadClient::~CrashpadClient() {}

bool CrashpadClient::StartCrashpadInProcessHandler() {
  InstallObjcExceptionPreprocessor();
  return SignalHandler::Get()->Install(nullptr);
}

// static
void CrashpadClient::DumpWithoutCrash() {
  DCHECK(SignalHandler::Get());
  siginfo_t siginfo = {};
  SignalHandler::Get()->HandleCrash(siginfo.si_signo, &siginfo, nullptr);
}
}  // namespace crashpad
