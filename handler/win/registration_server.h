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

#ifndef CRASHPAD_HANDLER_WIN_REGISTRATION_SERVER_H_
#define CRASHPAD_HANDLER_WIN_REGISTRATION_SERVER_H_

#include <windows.h>

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "util/win/address_types.h"
#include "util/win/scoped_handle.h"

namespace crashpad {

//! \brief Implements the server side of the Crashpad client registration
//!     protocol for Windows.
class RegistrationServer {
 public:
  //! \brief Handles registration requests.
  class Delegate {
   public:
    virtual ~Delegate() {}

    //! \brief Receives notification that clients may now connect to the named
    //!     pipe.
    virtual void OnStarted() = 0;

    //! \brief Responds to a request to register a client process for crash
    //!     handling.
    //!
    //! \param[in] client_process The client that is making the request.
    //! \param[in] crashpad_info_address The address of a CrashpadInfo structure
    //!     in the client's address space.
    //! \param[out] request_dump_event A `HANDLE`, valid in the client process,
    //!     to an event that, when signaled, triggers a crash report.
    //! \param[out] dump_complete_event A `HANDLE`, valid in the client process,
    //!     to an event that will be signaled when the crash report has been
    //!     successfully captured.
    virtual bool RegisterClient(ScopedKernelHANDLE client_process,
                                WinVMAddress crashpad_info_address,
                                HANDLE* request_dump_event,
                                HANDLE* dump_complete_event) = 0;
  };

  //! \brief Instantiates a client registration server.
  RegistrationServer();
  ~RegistrationServer();

  //! \brief Runs the RegistrationServer, receiving and processing requests and
  //!     sending responses. Blocks until Stop() is invoked.
  //!
  //! \param[in] pipe_name The pipe name to be used by the server.
  //! \param[in] delegate The delegate to be used to handle requests.
  //!
  //! \return Returns `true` if it terminates due to invocation of Stop().
  //!     `false` indicates early termination due to a failure.
  bool Run(const base::string16& pipe_name, Delegate* delegate);

  //! \brief Stops the RegistrationServer. Returns immediately. The instance
  //!     must not be destroyed until the call to Run() completes.
  void Stop();

 private:
  ScopedKernelHANDLE stop_event_;

  DISALLOW_COPY_AND_ASSIGN(RegistrationServer);
};

}  // namespace crashpad

#endif  // CRASHPAD_HANDLER_WIN_REGISTRATION_SERVER_H_
