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

#include "handler/mac/crash_report_upload_thread.h"

#include <errno.h>

#include <vector>

#include "base/logging.h"

namespace crashpad {

CrashReportUploadThread::CrashReportUploadThread(CrashReportDatabase* database)
    : database_(database),
      semaphore_(0),
      thread_(0),
      running_(false) {
}

CrashReportUploadThread::~CrashReportUploadThread() {
  DCHECK(!running_);
  DCHECK(!thread_);
}

void CrashReportUploadThread::Start() {
  DCHECK(!running_);
  DCHECK(!thread_);

  running_ = true;
  if ((errno = pthread_create(&thread_, nullptr, RunThreadMain, this)) != 0) {
    PLOG(ERROR) << "pthread_create";
    DCHECK(false);
    running_ = false;
  }
}

void CrashReportUploadThread::Stop() {
  DCHECK(running_);
  DCHECK(thread_);

  if (!running_) {
    return;
  }

  running_ = false;
  semaphore_.Signal();

  if ((errno = pthread_join(thread_, nullptr)) != 0) {
    PLOG(ERROR) << "pthread_join";
    DCHECK(false);
  }

  thread_ = 0;
}

void CrashReportUploadThread::ReportPending() {
  semaphore_.Signal();
}

void CrashReportUploadThread::ThreadMain() {
  while (running_) {
    ProcessPendingReports();

    // Check for pending reports every 15 minutes, even in the absence of a
    // signal from the handler thread. This allows for failed uploads to be
    // retried periodically, and for pending reports written by other processes
    // to be recognized.
    semaphore_.TimedWait(15 * 60);
  }
}

void CrashReportUploadThread::ProcessPendingReports() {
  std::vector<const CrashReportDatabase::Report> reports;
  if (database_->GetPendingReports(&reports) != CrashReportDatabase::kNoError) {
    // The database is sick. It might be prudent to stop trying to poke it from
    // this thread by abandoning the thread altogether. On the other hand, if
    // the problem is transient, it might be possible to talk to it again on the
    // next pass. For now, take the latter approach.
    return;
  }

  for (const CrashReportDatabase::Report& report : reports) {
    ProcessPendingReport(report);

    // Respect Stop() being called after at least one attempt to process a
    // report.
    if (!running_) {
      return;
    }
  }
}

void CrashReportUploadThread::ProcessPendingReport(
    const CrashReportDatabase::Report& report) {
  // TODO(mark): Actually upload the report, if uploads are enabled.
  database_->SkipReportUpload(report.uuid);
}

// static
void* CrashReportUploadThread::RunThreadMain(void* arg) {
  CrashReportUploadThread* self = static_cast<CrashReportUploadThread*>(arg);
  self->ThreadMain();
  return nullptr;
}

}  // namespace crashpad
