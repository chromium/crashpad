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

#include "client/crashpad_client.h"

#include <windows.h>

#include "base/atomicops.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "client/crashpad_info.h"
#include "client/registration_protocol_win.h"
#include "util/file/file_io.h"
#include "util/win/scoped_handle.h"

namespace {
// Time to wait for the handler to create a dump. This is tricky to figure out.
const DWORD kMillisecondsUntilTerminate = 5000;

// This is the exit code that the process will return to the system once the
// crash has been handled by Crashpad. We don't want to clash with the
// application-defined exit codes but we don't know them so we use one that is
// unlikely to be used.
const UINT kCrashExitCode = 0xffff7001;

// These two handles to events are leaked.
HANDLE g_signal_exception = nullptr;
HANDLE g_wait_termination = nullptr;

// Tracks whether a thread has already entered UnhandledExceptionHandler.
base::subtle::AtomicWord g_have_crashed;

LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS* exception_pointers) {
  // This is a per-process handler. While this handler is being invoked, other
  // threads are still executing as usual, so multiple threads could enter at
  // the same time. Because we're in a crashing state, we shouldn't be doing
  // anything that might cause allocations, call into kernel mode, etc. So, we
  // don't want to take a critical section here to avoid simultaneous access to
  // the global exception pointers in CrashpadInfo. Because the crash handler
  // will record all threads, it's fine to simply have the second and subsequent
  // entrants block here. They will soon be suspended by the crash handler, and
  // then the entire process will be terminated below. This means that we won't
  // save the exception pointers from the second and further crashes, but
  // contention here is very unlikely, and we'll still have a stack that's
  // blocked at this location.
  if (base::subtle::Barrier_AtomicIncrement(&g_have_crashed, 1) > 1) {
    SleepEx(INFINITE, false);
  }

  // Otherwise, we're the first thread, so record the exception pointer and
  // signal the crash handler.
  crashpad::CrashpadInfo::GetCrashpadInfo()->set_thread_id(
      GetCurrentThreadId());
  crashpad::CrashpadInfo::GetCrashpadInfo()->set_exception_pointers(
      exception_pointers);
  DWORD rv = SignalObjectAndWait(g_signal_exception,
                                 g_wait_termination,
                                 kMillisecondsUntilTerminate,
                                 false);
  if (rv != WAIT_OBJECT_0) {
    // Something went wrong. It is likely that a dump has not been created.
    if (rv == WAIT_TIMEOUT) {
      LOG(WARNING) << "SignalObjectAndWait timed out";
    } else {
      PLOG(WARNING) << "SignalObjectAndWait error";
    }
  }
  // We don't want to generate more exceptions, so we take the fast route.
  TerminateProcess(GetCurrentProcess(), kCrashExitCode);
  return 0L;
}

// Returns a pipe handle connected to the RegistrationServer.
crashpad::ScopedFileHANDLE Connect(const base::string16& pipe_name) {
  crashpad::ScopedFileHANDLE pipe;
  const int kMaxTries = 5;
  for (int tries = 0; tries < kMaxTries; ++tries) {
    pipe.reset(CreateFile(pipe_name.c_str(),
                          GENERIC_READ | GENERIC_WRITE,
                          0,
                          nullptr,
                          OPEN_EXISTING,
                          SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS,
                          nullptr));
    if (pipe.is_valid())
      break;

    // If busy, wait 60s before retrying.
    if (GetLastError() != ERROR_PIPE_BUSY) {
      PLOG(ERROR) << "CreateFile pipe connection";
      return crashpad::ScopedFileHANDLE();
    } else if (!WaitNamedPipe(pipe_name.c_str(), 60000)) {
      PLOG(ERROR) << "WaitNamedPipe";
    }
  }

  if (!pipe.is_valid())
    return crashpad::ScopedFileHANDLE();

  DWORD mode = PIPE_READMODE_MESSAGE;
  if (!SetNamedPipeHandleState(pipe.get(),
                               &mode,
                               nullptr,  // Don't set maximum bytes.
                               nullptr)) {  // Don't set maximum time.
    PLOG(ERROR) << "SetNamedPipeHandleState";
    return crashpad::ScopedFileHANDLE();
  }

  return pipe.Pass();
}

}  // namespace

namespace crashpad {

CrashpadClient::CrashpadClient() {
}

CrashpadClient::~CrashpadClient() {
}

bool CrashpadClient::StartHandler(
    const base::FilePath& handler,
    const base::FilePath& database,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    const std::vector<std::string>& arguments) {
  return false;
}

bool CrashpadClient::SetHandler(const std::string& ipc_port) {
  RegistrationRequest request = {0};
  request.client_process_id = GetCurrentProcessId();
  request.crashpad_info_address =
      reinterpret_cast<WinVMAddress>(CrashpadInfo::GetCrashpadInfo());

  RegistrationResponse response = {0};

  ScopedFileHANDLE pipe = Connect(base::UTF8ToUTF16(ipc_port));
  if (!pipe.is_valid())
    return false;
  bool result = LoggingWriteFile(pipe.get(), &request, sizeof(request)) &&
                LoggingReadFile(pipe.get(), &response, sizeof(response));
  if (!result)
    return result;

  // The server returns these already duplicated to be valid in this process.
  g_signal_exception = reinterpret_cast<HANDLE>(response.request_report_event);
  g_wait_termination =
      reinterpret_cast<HANDLE>(response.report_complete_event);
  return true;
}

bool CrashpadClient::UseHandler() {
  if (!g_signal_exception)
    return false;
  if (!g_wait_termination)
    return false;
  // In theory we could store the previous handler but it is not clear what
  // use we have for it.
  SetUnhandledExceptionFilter(&UnhandledExceptionHandler);
  return true;
}

}  // namespace crashpad
