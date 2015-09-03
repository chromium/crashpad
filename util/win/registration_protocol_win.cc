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

#include "util/win/registration_protocol_win.h"

#include <windows.h>

#include "base/logging.h"

namespace crashpad {

bool SendToCrashHandlerServer(const base::string16& pipe_name,
                              const crashpad::ClientToServerMessage& message,
                              crashpad::ServerToClientMessage* response) {
  int tries = 5;
  while (tries > 0) {
    HANDLE pipe = CreateFile(pipe_name.c_str(),
                             GENERIC_READ | GENERIC_WRITE,
                             0,
                             nullptr,
                             OPEN_EXISTING,
                             SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION,
                             nullptr);
    if (pipe == INVALID_HANDLE_VALUE) {
      Sleep(10);
      --tries;
      continue;
    }
    DWORD mode = PIPE_READMODE_MESSAGE;
    if (!SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr)) {
      PLOG(ERROR) << "SetNamedPipeHandleState";
      return false;
    }
    DWORD bytes_read = 0;
    BOOL result = TransactNamedPipe(
        pipe,
        // This is [in], but is incorrectly declared non-const.
        const_cast<crashpad::ClientToServerMessage*>(&message),
        sizeof(message),
        response,
        sizeof(*response),
        &bytes_read,
        nullptr);
    if (!result) {
      PLOG(ERROR) << "TransactNamedPipe";
      return false;
    }
    if (bytes_read != sizeof(*response)) {
      LOG(ERROR) << "TransactNamedPipe read incorrect number of bytes";
      return false;
    }
    return true;
  }

  LOG(ERROR) << "failed to connect after retrying";
  return false;
}

}  // namespace crashpad
