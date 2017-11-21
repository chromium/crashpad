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

#include <stdint.h>

#include <memory>
#include <unordered_map>

#include "base/macros.h"
#include "util/file/file_io.h"
#include "util/linux/registration_protocol.h"

namespace crashpad {

//! \brief Runs the main exception-handling server in Crashpad’s handler
//!     process.
class ExceptionHandlerServer {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    //! \brief Called on receipt of a crash dump request from a client.
    //!
    //! \param[in] client_process_id The process ID of the crashing client.
    //! \param[in] exception_information_address The address in the client's
    //!     address space of an ExceptionInformation struct.
    //! \return `true` on success. `false` on failure with a message logged.
    virtual bool HandleException(
        pid_t client_process_id,
        LinuxVMAddress exception_information_address) = 0;

    //! \brief Called on the receipt of a crash dump request from a client for a
    //!     crash that should be mediated by a PtraceBroker;
    //!
    //! \param[in] client_process_id The process ID of the crashing client.
    //! \param[in] exception_information_address The address in the client's
    //!     address space of an ExceptionInformation struct.
    //! \param[in] broker_sock A socket connected to the PtraceBroker.
    //! \return `true` on success. `false` on failure with a message logged.
    virtual bool HandleExceptionWithBroker(
        pid_t client_process_id,
        LinuxVMAddress exception_information_address,
        int broker_sock) = 0;
  };

  ExceptionHandlerServer();
  ~ExceptionHandlerServer();

  //! \brief Initializes this object with a connection to a client.
  //!
  //! This method must be successfully called before Run().
  //!
  //! \param[in] registration Registration information for the initial client.
  //! \param[in] sock A socket connected to the initial client.
  //! \return `true` on success. `false` on failure with a message logged.
  bool InitializeWithClient(const Registration& registration,
                            ScopedFileHandle sock);

  //! \brief Runs the exception-handling server.
  //!
  //! This method must only be called once on an ExceptionHandlerServer object.
  //! This method returns when there are no more client connections or Stop()
  //! has been called.
  //!
  //! \param[in] delegate An object to send exceptions to.
  void Run(Delegate* delegate);

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

  void HandleEvent(Event* event, uint32_t event_type);
  bool InstallClientRegistration(const Registration& registration,
                                 ScopedFileHandle socket);
  bool UninstallClientRegistration(Event* event);
  bool ReceiveClientMessage(Event* event);

  std::unordered_map<int, std::unique_ptr<Event>> clients_;
  std::unique_ptr<Event> shutdown_event_;
  Delegate* delegate_;
  ScopedFileHandle pollfd_;
  bool keep_running_;

  DISALLOW_COPY_AND_ASSIGN(ExceptionHandlerServer);
};

}  // namespace crashpad

#endif  // CRASHPAD_HANDLER_LINUX_EXCEPTION_HANDLER_SERVER_H_
