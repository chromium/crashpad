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

#ifndef CRASHPAD_CLIENT_REGISTRATION_PROTOCOL_WIN_H_
#define CRASHPAD_CLIENT_REGISTRATION_PROTOCOL_WIN_H_

#include <windows.h>
#include <stdint.h>

#include "util/win/address_types.h"

namespace crashpad {

#pragma pack(push, 1)
//! \brief A client registration request.
struct RegistrationRequest {
  //! \brief The PID of the client process.
  DWORD client_process_id;
  //! \brief The address, in the client process address space, of a CrashpadInfo
  //!     structure.
  crashpad::WinVMAddress crashpad_info_address;
};
#pragma pack(pop)

#pragma pack(push, 1)
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
  //! \brief An event `HANDLE`, valid in the client process, that will be
  //!     signaled when the requested crash report is complete. 64-bit clients
  //!     should convert the value to a `HANDLE` using sign-extension.
  uint32_t report_complete_event;
};
#pragma pack(pop)

}  // namespace crashpad

#endif  // CRASHPAD_CLIENT_REGISTRATION_PROTOCOL_WIN_H_
