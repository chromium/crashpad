// Copyright 2014 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_UTIL_MACH_CHILD_PORT_HANDSHAKE_H_
#define CRASHPAD_UTIL_MACH_CHILD_PORT_HANDSHAKE_H_

#include <mach/mach.h>

#include <string>

#include "base/basictypes.h"
#include "base/files/scoped_file.h"
#include "util/mach/child_port_server.h"

namespace crashpad {

namespace test {
namespace {
class ChildPortHandshakeTest;
}  // namespace
}  // namespace test

//! \brief Implements a handshake protocol that allows a parent process to
//!     obtain a Mach port right from a child process.
//!
//! Ordinarily, there is no way for parent and child processes to exchange port
//! rights, outside of the rights that children inherit from their parents.
//! These include task-special ports and exception ports, but all of these have
//! system-defined uses, and cannot reliably be replaced: in a multi-threaded
//! parent, it is impossible to temporarily change one an inheritable port while
//! maintaining a guarantee that another thread will not attempt to use it, and
//! in children, it difficult to guarantee that nothing will attempt to use an
//! inheritable port before it can be replaced with the correct one. This latter
//! concern is becoming increasingly more pronounced as system libraries perform
//! more operations that rely on an inheritable port in module initializers.
//!
//! The protocol implemented by this class involves a server that runs in the
//! parent process. The server is published with the bootstrap server, which the
//! child has access to because the bootstrap port is one of the inherited
//! task-special ports. The parent and child also share a pipe, which the parent
//! can write to and the child can read from. After launching a child process,
//! the parent will write a random token to this pipe, along with the name under
//! which its server has been registered with the bootstrap server. The child
//! can then obtain a send right to this server with `bootstrap_look_up()`, and
//! send a check-in message containing the token value and the port right of its
//! choice by calling `child_port_check_in()`.
//!
//! The inclusion of the token authenticates the child to its parent. This is
//! necessary because the service is published with the bootstrap server, which
//! opens up access to it to more than the child process. Because the token is
//! passed to the child by a shared pipe, it constitutes a shared secret not
//! known by other processes that may have incidental access to the server. The
//! ChildPortHandshake server considers its randomly-generated token valid until
//! a client checks in with it. This mechanism is used instead of examining the
//! request message’s audit trailer to verify the sender’s process ID because in
//! some process architectures, it may be impossible to verify the child’s
//! process ID. This may happen when the child disassociates from the parent
//! with a double fork(), and the actual client is the parent’s grandchild. In
//! this case, the child would not check in, but the grandchild, in possession
//! of the token, would check in.
//!
//! The shared pipe serves another purpose: the server monitors it for an
//! end-of-file (no readers) condition. Once detected, it will stop its blocking
//! wait for a client to check in. This mechanism was chosen over monitoring a
//! child process directly for exit to account for the possibility that the
//! child might disassociate with a double fork().
//!
//! This class can be used to allow a child process to provide its parent with
//! a send right to its task port, in cases where it is desirable for the parent
//! to have such access. It can also be used to allow a child process to
//! establish its own server and provide its parent with a send right to that
//! server, for cases where a service is provided and it is undesirable or
//! impossible to provide it via the bootstrap or launchd interfaces.
class ChildPortHandshake : public ChildPortServer::Interface {
 public:
  //! \brief Initializes the server.
  //!
  //! This creates the pipe so that the “read” side can be obtained by calling
  //! ReadPipeFD().
  ChildPortHandshake();

  ~ChildPortHandshake();

  //! \brief Obtains the “read” side of the pipe, to be used by the client.
  //!
  //! Callers must obtain this file descriptor and arrange for the caller to
  //! have access to it before calling RunServer().
  //!
  //! \return The file descriptor that the client should read from.
  int ReadPipeFD() const;

  //! \brief Runs the server.
  //!
  //! This method performs these tasks:
  //!  - Closes the “read” side of the pipe in-process, so that the client
  //!    process holds the only file descriptor that can read from the pipe.
  //!  - Creates a random token and sends it via the pipe.
  //!  - Checks its service in with the bootstrap server, and sends the name
  //!    of its bootstrap service mapping via the pipe.
  //!  - Simultaneously receives messages on its Mach server and monitors the
  //!    pipe for end-of-file. This is a blocking operation.
  //!  - When a Mach message is received, calls HandleChildPortCheckIn() to
  //!    interpret and validate it, and if the message is valid, returns the
  //!    port right extracted from the message. If the message is not valid,
  //!    this method will continue waiting for a valid message. Valid messages
  //!    are properly formatted and have the correct token. If a valid message
  //!    carries a send or send-once right, it will be returned. If a valid
  //!    message contains a receive right, it will be destroyed and
  //!    `MACH_PORT_NULL` will be returned. If a message is not valid, this
  //!    method will continue waiting for pipe EOF or a valid message.
  //!  - When notified of pipe EOF, returns `MACH_PORT_NULL`.
  //!  - Regardless of return value, destroys the server’s receive right and
  //!    closes the pipe.
  //!
  //! \return On success, the send or send-once right to the port provided by
  //!     the client. The caller takes ownership of this right. On failure,
  //!     `MACH_PORT_NULL`, indicating that the client did not check in properly
  //!     before terminating, where termination is detected by noticing that the
  //!     read side of the shared pipe has closed. On failure, a message
  //!     indiciating the nature of the failure will be logged.
  mach_port_t RunServer();

  // ChildPortServer::Interface:
  kern_return_t HandleChildPortCheckIn(child_port_server_t server,
                                       child_port_token_t token,
                                       mach_port_t port,
                                       mach_msg_type_name_t right_type,
                                       const mach_msg_trailer_t* trailer,
                                       bool* destroy_request) override;

  //! \brief Runs the client.
  //!
  //! This function performs these tasks:
  //!  - Reads the token from the pipe.
  //!  - Reads the bootstrap service name from the pipe.
  //!  - Obtains a send right to the server by calling `bootstrap_look_up()`.
  //!  - Sends a check-in message to the server by calling
  //!    `child_port_check_in()`, providing the token and the user-supplied port
  //!    right.
  //!  - Deallocates the send right to the server, and closes the pipe.
  //!
  //! There is no return value because `child_port_check_in()` is a MIG
  //! `simpleroutine`, and the server does not send a reply. This allows
  //! check-in to occur without blocking to wait for a reply.
  //!
  //! \param[in] pipe_read The “read” side of the pipe shared with the server
  //!     process.
  //! \param[in] port The port that will be passed to the server by
  //!     `child_port_check_in()`.
  //! \param[in] right_type The right type to furnish the parent with. If \a
  //!     port is a send right, this can be `MACH_MSG_TYPE_COPY_SEND` or
  //!     `MACH_MSG_TYPE_MOVE_SEND`. If \a port is a send-once right, this can
  //!      be `MACH_MSG_TYPE_MOVE_SEND_ONCE`. If \a port is a receive right,
  //!      this can be `MACH_MSG_TYPE_MAKE_SEND`. `MACH_MSG_TYPE_MOVE_RECEIVE`
  //!      is supported by the client interface but will be silently rejected by
  //!      server run by RunServer(), which expects to receive only send or
  //!      send-once rights.
  static void RunClient(int pipe_read,
                        mach_port_t port,
                        mach_msg_type_name_t right_type);

 private:
  //! \brief Runs the read-from-pipe portion of the client’s side of the
  //!     handshake. This is an implementation detail of RunClient and is only
  //!     exposed for testing purposes.
  //!
  //! \param[in] pipe_read The “read” side of the pipe shared with the server
  //!     process.
  //! \param[out] token The token value read from \a pipe_read.
  //! \param[out] service_name The service name as registered with the bootstrap
  //!     server, read from \a pipe_read.
  static void RunClientInternal_ReadPipe(int pipe_read,
                                         child_port_token_t* token,
                                         std::string* service_name);

  //! \brief Runs the check-in portion of the client’s side of the handshake.
  //!     This is an implementation detail of RunClient and is only exposed for
  //!     testing purposes.
  //!
  //! \param[in] service_name The service name as registered with the bootstrap
  //!     server, to be looked up with `bootstrap_look_up()`.
  //! \param[in] token The token value to provide during check-in.
  //! \param[in] port The port that will be passed to the server by
  //!     `child_port_check_in()`.
  //! \param[in] right_type The right type to furnish the parent with.
  static void RunClientInternal_SendCheckIn(const std::string& service_name,
                                            child_port_token_t token,
                                            mach_port_t port,
                                            mach_msg_type_name_t right_type);

  // Communicates the token from RunServer(), where it’s generated, to
  // HandleChildPortCheckIn(), where it’s validated.
  child_port_token_t token_;

  base::ScopedFD pipe_read_;
  base::ScopedFD pipe_write_;

  // Communicates the port received from the client from
  // HandleChildPortCheckIn(), where it’s received, to RunServer(), where it’s
  // returned. This is strongly-owned, but ownership is transferred to
  // RunServer()’s caller.
  mach_port_t child_port_;

  // Communicates that a check-in with a valid token was received by
  // HandleChildPortCheckIn(), and that the value of child_port_ should be
  // returned to RunServer()’s caller.
  bool checked_in_;

  friend class test::ChildPortHandshakeTest;

  DISALLOW_COPY_AND_ASSIGN(ChildPortHandshake);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_MACH_CHILD_PORT_HANDSHAKE_H_
