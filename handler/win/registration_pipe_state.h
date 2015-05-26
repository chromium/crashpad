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

#ifndef CRASHPAD_HANDLER_WIN_REGISTRATION_PIPE_STATE_H_
#define CRASHPAD_HANDLER_WIN_REGISTRATION_PIPE_STATE_H_

#include <windows.h>

#include "base/basictypes.h"
#include "client/registration_protocol_win.h"
#include "handler/win/registration_server.h"
#include "util/win/scoped_handle.h"

namespace crashpad {

//! \brief Implements the state and state transitions of a single named pipe for
//!     the Crashpad client registration protocol for Windows. Each pipe
//!     instance may handle a single client connection at a time. After each
//!     connection completes, whether successfully or otherwise, the instance
//!     will attempt to return to a listening state, ready for a new client
//!     connection.
class RegistrationPipeState {
 public:
  //! \brief Initializes the state for a single pipe instance. The client must
  //!     call Initialize() before clients may connect to this pipe.
  //! \param[in] pipe The named pipe to listen on.
  //! \param[in] delegate The delegate that will be used to handle requests.
  RegistrationPipeState(ScopedFileHANDLE pipe,
                        RegistrationServer::Delegate* delegate);
  ~RegistrationPipeState();

  //! \brief Places the pipe in the running state, ready to receive and process
  //!     client connections. Before destroying a running RegistrationPipeState
  //!     instance you must invoke Stop() and then wait for completion_event()
  //!     to be signaled one last time.
  //! \return Returns true if successful, in which case the client must observe
  //!     completion_event() and invoke OnCompletion() whenever it is signaled.
  bool Initialize();

  //! \brief Cancels any pending asynchronous operations. After invoking this
  //!     method you must wait for completion_event() to be signaled before
  //!     destroying the instance.
  void Stop();

  //! \brief Returns an event handle that will be signaled whenever an
  //!     asynchronous operation associated with this instance completes.
  HANDLE completion_event() { return event_.get(); }

  //! \brief Must be called by the client whenever completion_event() is
  //!     signaled.
  //! \return Returns true if the pipe is still in the running state. Otherwise,
  //!     a permanent failure has occurred and the instance may be immediately
  //!     destroyed.
  bool OnCompletion();

 private:
  using AsyncCompletionHandler =
      bool (RegistrationPipeState::*)(DWORD bytes_transferred);

  // State transition handlers. Return true if the pipe is still valid.

  bool OnConnectComplete(DWORD /* bytes_transferred */);
  bool OnReadComplete(DWORD bytes_transferred);
  bool OnWriteComplete(DWORD bytes_transferred);
  bool OnWaitForClientCloseComplete(DWORD bytes_transferred);

  // Pipe operations. Return true if the pipe is still valid.

  // Prepares the pipe to accept a new client connecion.
  bool IssueConnect();
  // Reads into |request_|.
  bool IssueRead();
  // Writes from |response_|.
  bool IssueWrite();
  // Issues a final ReadFile() that is expected to be terminated when the client
  // closes the pipe.
  bool IssueWaitForClientClose();
  // Processes |request_| using |delegate_| and stores the result in
  // |response_|.
  bool HandleRequest();
  // Closes the active connection and invokes IssueConnect().
  bool ResetConnection();

  RegistrationRequest request_;
  RegistrationResponse response_;
  // The state transition handler to be invoked when the active asynchronous
  // operation completes.
  AsyncCompletionHandler completion_handler_;
  OVERLAPPED overlapped_;
  ScopedKernelHANDLE event_;
  ScopedFileHANDLE pipe_;
  bool waiting_for_close_;
  RegistrationServer::Delegate* delegate_;
  decltype(GetNamedPipeClientProcessId)* get_named_pipe_client_process_id_proc_;

  DISALLOW_COPY_AND_ASSIGN(RegistrationPipeState);
};

}  // namespace crashpad

#endif  // CRASHPAD_HANDLER_WIN_REGISTRATION_PIPE_STATE_H_
