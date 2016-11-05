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
#include <sddl.h>

#include "base/logging.h"
#include "util/win/exception_handler_server.h"
#include "util/win/scoped_handle.h"
#include "util/win/scoped_local_alloc.h"

namespace crashpad {

bool SendToCrashHandlerServer(const base::string16& pipe_name,
                              const ClientToServerMessage& message,
                              ServerToClientMessage* response) {
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
  for (;;) {
    ScopedFileHANDLE pipe(
        CreateFile(pipe_name.c_str(),
                   GENERIC_READ | GENERIC_WRITE,
                   0,
                   nullptr,
                   OPEN_EXISTING,
                   SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION,
                   nullptr));
    if (!pipe.is_valid()) {
      if (GetLastError() != ERROR_PIPE_BUSY) {
        PLOG(ERROR) << "CreateFile";
        return false;
      }

      if (!WaitNamedPipe(pipe_name.c_str(), NMPWAIT_WAIT_FOREVER)) {
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
        const_cast<ClientToServerMessage*>(&message),
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

HANDLE CreateNamedPipeInstance(const std::wstring& pipe_name,
                               bool first_instance) {
  SECURITY_ATTRIBUTES security_attributes;
  SECURITY_ATTRIBUTES* security_attributes_pointer = nullptr;
  ScopedLocalAlloc scoped_sec_desc;

  if (first_instance) {
    // Pre-Vista does not have integrity levels.
    const DWORD version = GetVersion();
    const DWORD major_version = LOBYTE(LOWORD(version));
    const bool is_vista_or_later = major_version >= 6;
    if (is_vista_or_later) {
      // Mandatory Label, no ACE flags, no ObjectType, integrity level
      // untrusted.
      const wchar_t kSddl[] = L"S:(ML;;;;;S-1-16-0)";

      PSECURITY_DESCRIPTOR sec_desc;
      PCHECK(ConvertStringSecurityDescriptorToSecurityDescriptor(
          kSddl, SDDL_REVISION_1, &sec_desc, nullptr))
          << "ConvertStringSecurityDescriptorToSecurityDescriptor";

      // Take ownership of the allocated SECURITY_DESCRIPTOR.
      scoped_sec_desc.reset(sec_desc);

      memset(&security_attributes, 0, sizeof(security_attributes));
      security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
      security_attributes.lpSecurityDescriptor = sec_desc;
      security_attributes.bInheritHandle = TRUE;
      security_attributes_pointer = &security_attributes;
    }
  }

  return CreateNamedPipe(
      pipe_name.c_str(),
      PIPE_ACCESS_DUPLEX | (first_instance ? FILE_FLAG_FIRST_PIPE_INSTANCE : 0),
      PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
      ExceptionHandlerServer::kPipeInstances,
      512,
      512,
      0,
      security_attributes_pointer);
}

}  // namespace crashpad
