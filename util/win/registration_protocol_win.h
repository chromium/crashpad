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

#ifndef CRASHPAD_UTIL_WIN_REGISTRATION_PROTOCOL_WIN_H_
#define CRASHPAD_UTIL_WIN_REGISTRATION_PROTOCOL_WIN_H_

#include <windows.h>
#include <stdint.h>

#include "base/strings/string16.h"
#include "util/win/address_types.h"

namespace crashpad {

#pragma pack(push, 1)

//! \brief Structure read out of the client process by the crash handler when an
//!     exception occurs.
struct ExceptionInformation {
  //! \brief The address of an EXCEPTION_POINTERS structure in the client
  //!     process that describes the exception.
  WinVMAddress exception_pointers;

  //! \brief The thread on which the exception happened.
  DWORD thread_id;
};

//! \brief A client registration request.
struct RegistrationRequest {
  //! \brief The address, in the client process address space, of an
  //!     ExceptionInformation structure.
  WinVMAddress exception_information;

  //! \brief The PID of the client process.
  DWORD client_process_id;
};

//! \brief A message only sent to the server by itself to trigger shutdown.
struct ShutdownRequest {
  //! \brief A randomly generated token used to validate the the shutdown
  //!     request was not sent from another process.
  uint64_t token;
};

//! \brief The message passed from client to server by
//!     SendToCrashHandlerServer().
struct ClientToServerMessage {
  //! \brief Indicates which field of the union is in use.
  enum Type : uint32_t {
    //! \brief For RegistrationRequest.
    kRegister,
    //! \brief For ShutdownRequest.
    kShutdown,
  } type;

  union {
    RegistrationRequest registration;
    ShutdownRequest shutdown;
  };
};

//! \brief  A client registration response.
//!
//! See <a
//! href="https://msdn.microsoft.com/en-us/library/windows/desktop/aa384203">Interprocess
//! Communication Between 32-bit and 64-bit Applications</a> for details on
//! communicating handle values between processes of varying bitness.
struct RegistrationResponse {
  //! \brief An event `HANDLE`, valid in the client process, that should be
  //!     signaled to request a crash report. 64-bit clients should convert the
  //!     value to a `HANDLE` using sign-extension.
  uint32_t request_report_event;
};

//! \brief The response sent back to the client via SendToCrashHandlerServer().
union ServerToClientMessage {
  RegistrationResponse registration;
};

#pragma pack(pop)

//! \brief Connect over the given \a pipe_name, passing \a message to the
//!     server, storing the server's reply into \a response.
//!
//! Typically clients will not use this directly, instead using
//! CrashpadClient::SetHandler().
//!
//! \sa CrashpadClient::SetHandler()
bool SendToCrashHandlerServer(const base::string16& pipe_name,
                              const ClientToServerMessage& message,
                              ServerToClientMessage* response);

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_WIN_REGISTRATION_PROTOCOL_WIN_H_
