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

#ifndef CRASHPAD_UTIL_LINUX_REGISTRATION_PROTOCOL_H_
#define CRASHPAD_UTIL_LINUX_REGISTRATION_PROTOCOL_H_

#include <stdint.h>
#include <sys/types.h>

#include "util/file/file_io.h"
#include "util/linux/address_types.h"

namespace crashpad {

#pragma pack(push, 1)

//! \brief Information about a client registered with an ExceptionHandlerServer.
struct Registration {
  //! \brief The address in the client's address space of an
  //!     ExceptionInformation struct.
  LinuxVMAddress exception_information_address;

  //! \brief The process ID of the client.
  pid_t client_process_id;

  //! \brief `true` if a PtraceBroker should be used to trace this client.
  bool use_broker;
};

//! \brief The message passed from client to server
struct ClientToServerMessage {
  static constexpr int32_t kVersion = 1;

  //! \brief Indicates what message version is being used.
  int32_t version = kVersion;

  enum Type : uint32_t {
    //! \brief Used to register new clients with the server.
    kRegistration,

    //! \brief Used to request a crash dump for the sending client.
    kCrashDumpRequest
  } type;

  union {
    //! \brief Valid for type == kRegistration.
    Registration registration;
  };
};

#pragma pack(pop)

//! \brief Registers a client with an ExceptionHandlerServer.
//!
//! \param[in] server_sock An existing client connection to the server.
//! \param[in] registration The registration info for the new client.
//! \param[in] registration_socket The socket that the server should associate
//!     with this registration. Ownership is transferred to the
//!     server and the socket is invalided as part of this call part of this
//!     call.
//! \return `true` on success. `false` on failure with a message logged.
bool RegisterWithHandler(int server_sock,
                         const Registration& registration,
                         ScopedFileHandle registration_socket);

//! \brief Signal a crash dump request on a socket.
//!
//! This method sends a request to the server but does not block for the server
//! to complete the request. The caller can call WaitForCrashDump to get the
//! success status of this operation.
//!
//! \param[in] registration_socket The client half of a connected socket pair
//!     associated with this registration.
//! \return `true` on success. `false` on failure with a message logged.
bool RequestCrashDump(int registration_socket);

//! \brief Block until the server has completed the crash dump.
//!
//! \param[in] registration_socket The client half of a connected socket pair
//!     associated with this registration.
//! \return `true` if the crash dump was a success. Otherwise `false`.
bool WaitForCrashDump(int registration_socket);

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_LINUX_REGISTRATION_PROTOCOL_H_
