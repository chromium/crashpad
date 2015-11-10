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
#include "util/win/scoped_handle.h"

namespace crashpad {

bool SendToCrashHandlerServer(const base::string16& pipe_name,
                              const crashpad::ClientToServerMessage& message,
                              crashpad::ServerToClientMessage* response) {
  // Retry CreateFile() in a loop. If the handler isn’t actively waiting in
  // ConnectNamedPipe() on a pipe instance because it’s busy doing something
  // else, CreateFile() will fail with ERROR_PIPE_BUSY. WaitNamedPipe() waits
  // until a pipe instance is ready, but there’s no way to wait for this
  // condition and atomically open the client side of the pipe in a single
  // operation. CallNamedPipe() implements similar retry logic to this, also in
  // user-mode code.
  //
  // This loop is only intended to retry on ERROR_PIPE_BUSY. Notably, if the
  // handler is so lazy that it hasn’t even called CreateNamedPipe() yet,
  // CreateFile() will fail with ERROR_FILE_NOT_FOUND, and this function is
  // expected to fail without retrying anything. If the handler is started at
  // around the same time as its client, something external to this code must be
  // done to guarantee correct ordering. When the client starts the handler
  // itself, CrashpadClient::StartHandler() provides this synchronization.
  for (int tries = 0;;) {
    ScopedFileHANDLE pipe(
        CreateFile(pipe_name.c_str(),
                   GENERIC_READ | GENERIC_WRITE,
                   0,
                   nullptr,
                   OPEN_EXISTING,
                   SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION,
                   nullptr));
    if (!pipe.is_valid()) {
      if (++tries == 5 || GetLastError() != ERROR_PIPE_BUSY) {
        PLOG(ERROR) << "CreateFile";
        return false;
      }

      if (!WaitNamedPipe(pipe_name.c_str(), 1000) &&
          GetLastError() != ERROR_SEM_TIMEOUT) {
        PLOG(ERROR) << "WaitNamedPipe";
        return false;
      }

      continue;
    }

    DWORD mode = PIPE_READMODE_MESSAGE;
    if (!SetNamedPipeHandleState(pipe.get(), &mode, nullptr, nullptr)) {
      PLOG(ERROR) << "SetNamedPipeHandleState";
      return false;
    }
    DWORD bytes_read = 0;
    BOOL result = TransactNamedPipe(
        pipe.get(),
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
      LOG(ERROR) << "TransactNamedPipe: expected " << sizeof(*response)
                 << ", observed " << bytes_read;
      return false;
    }
    return true;
  }
}

}  // namespace crashpad
