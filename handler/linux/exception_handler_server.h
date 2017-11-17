// Copyright 2017 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_HANDLER_LINUX_EXCEPTION_HANDLER_SERVER_H_
#define CRASHPAD_HANDLER_LINUX_EXCEPTION_HANDLER_SERVER_H_

#include <signal.h>
#include <sys/types.h>

#include <unordered_map>

#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "util/file/file_io.h"
#include "util/linux/address_types.h"
#include "util/linux/registration_protocol.h"
#include "util/synchronization/semaphore.h"

namespace crashpad {

//! \brief Runs the main exception-handling server in Crashpad’s handler
//!     process.
class ExceptionHandlerServer {
 public:
  //! \brief Handles exceptions
  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual void HandleException(
        pid_t client_process_id,
        LinuxVMAddress exception_information_address,
        bool use_broker) = 0;
  };

  ExceptionHandlerServer();
  ~ExceptionHandlerServer();

  //! \brief Initializes this object with a connection to a client.
  //!
  //! This method must be successfully called before any other in this class.
  //!
  //! \param[in] sock A socket connected to the initial client.
  bool InitializeWithClient(const Registration& registration,
                            ScopedFileHandle sock);

  //! \brief Runs the exception-handling server.
  //!
  //! \param[in] delegate An object to send exceptions to.
  //!
  //! This method must only be called once on an ExceptionHandlerServer object.
  void Run(Delegate* delegate);

  bool WaitUntilReady(double seconds);

  //! \brief Stops a running exception-handling server.
  //!
  //! The normal mode of operation is to call Stop() while Run() is running. It
  //! is expected that Stop() would be called from a signal handler.
  //!
  //! If Stop() is called before Run() it will cause Run() to return as soon as
  //! it is called. It is harmless to call Stop() after Run() has already
  //! returned, or to call Stop() after it has already been called.
  void Stop();

 private:
  struct Event;

  bool SendSelfShutdown();
  bool InstallEvent(std::unique_ptr<Event> event);
  bool UninstallEvent(Event* event);
  bool InstallController(ScopedFileHandle socket);
  bool InstallClientSocket(const Registration& registration,
                                 ScopedFileHandle socket);
  void HandleEvent(Event* event, uint32_t event_type);
  void ReceiveControlMessage(Event* event);
  void ReceiveClientMessage(Event* event);

  std::unordered_map<int, std::unique_ptr<Event>> events_;
  Delegate* delegate_;
  ScopedFileHandle pollfd_;
  Semaphore ready_;
  int controller_fd_;
  bool ok_to_run_;

  DISALLOW_COPY_AND_ASSIGN(ExceptionHandlerServer);
};

}  // namespace crashpad

#endif  // CRASHPAD_HANDLER_LINUX_EXCEPTION_HANDLER_SERVER_H_
