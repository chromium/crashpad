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

#include "base/logging.h"

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

LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS* exception_pointers) {
  // TODO (cpu): Here write |exception_pointers| to g_crashpad_info.
  DWORD rv = SignalObjectAndWait(g_signal_exception,
                                 g_wait_termination,
                                 kMillisecondsUntilTerminate,
                                 FALSE);
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
  // TODO(cpu): Provide a reference implementation.
  return false;
}

bool CrashpadClient::SetHandler(const std::string& ipc_port) {
  // TODO (cpu): Contact the handler and obtain g_signal_exception and
  // g_wait_termination.
  return false;
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
