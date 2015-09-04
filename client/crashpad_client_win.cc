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

#include <string.h>
#include <windows.h>

#include "base/atomicops.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "util/file/file_io.h"
#include "util/win/registration_protocol_win.h"
#include "util/win/scoped_handle.h"

namespace {

// This handle is never closed.
HANDLE g_signal_exception = INVALID_HANDLE_VALUE;

// Where we store the exception information that the crash handler reads.
crashpad::ExceptionInformation g_exception_information;

LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS* exception_pointers) {
  // Tracks whether a thread has already entered UnhandledExceptionHandler.
  static base::subtle::AtomicWord have_crashed;

  // This is a per-process handler. While this handler is being invoked, other
  // threads are still executing as usual, so multiple threads could enter at
  // the same time. Because we're in a crashing state, we shouldn't be doing
  // anything that might cause allocations, call into kernel mode, etc. So, we
  // don't want to take a critical section here to avoid simultaneous access to
  // the global exception pointers in ExceptionInformation. Because the crash
  // handler will record all threads, it's fine to simply have the second and
  // subsequent entrants block here. They will soon be suspended by the crash
  // handler, and then the entire process will be terminated below. This means
  // that we won't save the exception pointers from the second and further
  // crashes, but contention here is very unlikely, and we'll still have a stack
  // that's blocked at this location.
  if (base::subtle::Barrier_AtomicIncrement(&have_crashed, 1) > 1) {
    SleepEx(INFINITE, false);
  }

  // Otherwise, we're the first thread, so record the exception pointer and
  // signal the crash handler.
  g_exception_information.thread_id = GetCurrentThreadId();
  g_exception_information.exception_pointers =
      reinterpret_cast<crashpad::WinVMAddress>(exception_pointers);

  // Now signal the crash server, which will take a dump and then terminate us
  // when it's complete.
  SetEvent(g_signal_exception);

  // Time to wait for the handler to create a dump.
  const DWORD kMillisecondsUntilTerminate = 60 * 1000;

  // Sleep for a while to allow it to process us. Eventually, we terminate
  // ourselves in case the crash server is gone, so that we don't leave zombies
  // around. This would ideally never happen.
  // TODO(scottmg): Re-add the "reply" event here, for implementing
  // DumpWithoutCrashing.
  Sleep(kMillisecondsUntilTerminate);

  LOG(ERROR) << "crash server did not respond, self-terminating";

  const UINT kCrashExitCodeNoDump = 0xffff7001;
  TerminateProcess(GetCurrentProcess(), kCrashExitCodeNoDump);

  return EXCEPTION_CONTINUE_SEARCH;
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
  LOG(FATAL) << "SetHandler should be used on Windows";
  return false;
}

bool CrashpadClient::SetHandler(const std::string& ipc_port) {
  ClientToServerMessage message;
  memset(&message, 0, sizeof(message));
  message.type = ClientToServerMessage::kRegister;
  message.registration.version = RegistrationRequest::kMessageVersion;
  message.registration.client_process_id = GetCurrentProcessId();
  message.registration.exception_information =
      reinterpret_cast<WinVMAddress>(&g_exception_information);

  ServerToClientMessage response = {0};

  if (!SendToCrashHandlerServer(
          base::UTF8ToUTF16(ipc_port), message, &response)) {
    return false;
  }

  // The server returns these already duplicated to be valid in this process.
  g_signal_exception =
      reinterpret_cast<HANDLE>(response.registration.request_report_event);
  return true;
}

bool CrashpadClient::UseHandler() {
  if (g_signal_exception == INVALID_HANDLE_VALUE)
    return false;
  // In theory we could store the previous handler but it is not clear what
  // use we have for it.
  SetUnhandledExceptionFilter(&UnhandledExceptionHandler);
  return true;
}

}  // namespace crashpad
