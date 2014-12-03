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

#include "util/mach/child_port_handshake.h"

#include <errno.h>
#include <pthread.h>
#include <servers/bootstrap.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>

#include "base/logging.h"
#include "base/mac/mach_logging.h"
#include "base/mac/scoped_mach_port.h"
#include "base/posix/eintr_wrapper.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "util/file/fd_io.h"
#include "util/mach/child_port.h"
#include "util/mach/mach_extensions.h"
#include "util/mach/mach_message_server.h"

namespace crashpad {

ChildPortHandshake::ChildPortHandshake()
    : token_(0),
      pipe_read_(),
      pipe_write_(),
      child_port_(MACH_PORT_NULL),
      checked_in_(false) {
  // Use socketpair() instead of pipe(). There is no way to suppress SIGPIPE on
  // pipes in Mac OS X 10.6, because the F_SETNOSIGPIPE fcntl() command was not
  // introduced until 10.7.
  int pipe_fds[2];
  PCHECK(socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, pipe_fds) == 0)
      << "socketpair";

  pipe_read_.reset(pipe_fds[0]);
  pipe_write_.reset(pipe_fds[1]);

  // Simulate pipe() semantics by shutting down the “wrong” sides of the socket.
  PCHECK(shutdown(pipe_write_.get(), SHUT_RD) == 0) << "shutdown";
  PCHECK(shutdown(pipe_read_.get(), SHUT_WR) == 0) << "shutdown";

  // SIGPIPE is undesirable when writing to this pipe. Allow broken-pipe writes
  // to fail with EPIPE instead.
  const int value = 1;
  PCHECK(setsockopt(
      pipe_write_.get(), SOL_SOCKET, SO_NOSIGPIPE, &value, sizeof(value)) == 0)
      << "setsockopt";
}

ChildPortHandshake::~ChildPortHandshake() {
}

int ChildPortHandshake::ReadPipeFD() const {
  DCHECK_NE(pipe_read_.get(), -1);
  return pipe_read_.get();
}

mach_port_t ChildPortHandshake::RunServer() {
  DCHECK_NE(pipe_read_.get(), -1);
  pipe_read_.reset();

  // Transfer ownership of the write pipe into this method’s scope.
  base::ScopedFD pipe_write_owner(pipe_write_.release());

  // Initialize the token and share it with the client via the pipe.
  token_ = base::RandUint64();
  int pipe_write = pipe_write_owner.get();
  if (!LoggingWriteFD(pipe_write, &token_, sizeof(token_))) {
    LOG(WARNING) << "no client check-in";
    return MACH_PORT_NULL;
  }

  // Create a unique name for the bootstrap service mapping. Make it unguessable
  // to prevent outsiders from grabbing the name first, which would cause
  // bootstrap_check_in() to fail.
  uint64_t thread_id;
  errno = pthread_threadid_np(pthread_self(), &thread_id);
  PCHECK(errno == 0) << "pthread_threadid_np";
  std::string service_name = base::StringPrintf(
      "com.googlecode.crashpad.child_port_handshake.%d.%llu.%016llx",
      getpid(),
      thread_id,
      base::RandUint64());
  DCHECK_LT(service_name.size(), implicit_cast<size_t>(BOOTSTRAP_MAX_NAME_LEN));

  // Check the new service in with the bootstrap server, obtaining a receive
  // right for it.
  mach_port_t server_port;
  kern_return_t kr =
      bootstrap_check_in(bootstrap_port, service_name.c_str(), &server_port);
  BOOTSTRAP_CHECK(kr == BOOTSTRAP_SUCCESS, kr) << "bootstrap_check_in";
  base::mac::ScopedMachReceiveRight server_port_owner(server_port);

  // Share the service name with the client via the pipe.
  uint32_t service_name_length = service_name.size();
  if (!LoggingWriteFD(
          pipe_write, &service_name_length, sizeof(service_name_length))) {
    LOG(WARNING) << "no client check-in";
    return MACH_PORT_NULL;
  }

  if (!LoggingWriteFD(pipe_write, service_name.c_str(), service_name_length)) {
    LOG(WARNING) << "no client check-in";
    return MACH_PORT_NULL;
  }

  // A kqueue cannot monitor a raw Mach receive right with EVFILT_MACHPORT. It
  // requires a port set. Create a new port set and add the receive right to it.
  mach_port_t server_port_set;
  kr = mach_port_allocate(
      mach_task_self(), MACH_PORT_RIGHT_PORT_SET, &server_port_set);
  MACH_CHECK(kr == KERN_SUCCESS, kr) << "mach_port_allocate";
  base::mac::ScopedMachPortSet server_port_set_owner(server_port_set);

  kr = mach_port_insert_member(mach_task_self(), server_port, server_port_set);
  MACH_CHECK(kr == KERN_SUCCESS, kr) << "mach_port_insert_member";

  // Set up a kqueue to monitor both the server’s receive right and the write
  // side of the pipe. Messages from the client will be received via the receive
  // right, and the pipe will show EOF if the client closes its read side
  // prematurely.
  base::ScopedFD kq(kqueue());
  PCHECK(kq != -1) << "kqueue";

  struct kevent changelist[2];
  EV_SET(&changelist[0],
         server_port_set,
         EVFILT_MACHPORT,
         EV_ADD | EV_CLEAR,
         0,
         0,
         nullptr);
  EV_SET(&changelist[1],
         pipe_write,
         EVFILT_WRITE,
         EV_ADD | EV_CLEAR,
         0,
         0,
         nullptr);
  int rv = HANDLE_EINTR(
      kevent(kq.get(), changelist, arraysize(changelist), nullptr, 0, nullptr));
  PCHECK(rv != -1) << "kevent";

  ChildPortServer child_port_server(this);

  bool blocking = true;
  DCHECK(!checked_in_);
  while (!checked_in_) {
    DCHECK_EQ(child_port_, kMachPortNull);

    // Get a kevent from the kqueue. Block while waiting for an event unless the
    // write pipe has arrived at EOF, in which case the kevent() should be
    // nonblocking. Although the client sends its check-in message before
    // closing the read side of the pipe, this organization allows the events to
    // be delivered out of order and the check-in message will still be
    // processed.
    struct kevent event;
    const timespec nonblocking_timeout = {};
    const timespec* timeout = blocking ? nullptr : &nonblocking_timeout;
    rv = HANDLE_EINTR(kevent(kq.get(), nullptr, 0, &event, 1, timeout));
    PCHECK(rv != -1) << "kevent";

    if (rv == 0) {
      // Non-blocking kevent() with no events to return.
      DCHECK(!blocking);
      LOG(WARNING) << "no client check-in";
      return MACH_PORT_NULL;
    }

    DCHECK_EQ(rv, 1);

    if (event.flags & EV_ERROR) {
      // kevent() may have put its error here.
      errno = event.data;
      PLOG(FATAL) << "kevent";
    }

    switch (event.filter) {
      case EVFILT_MACHPORT: {
        // There’s something to receive on the port set.
        DCHECK_EQ(event.ident, server_port_set);

        // Run the message server in an inner loop instead of using
        // MachMessageServer::kPersistent. This allows the loop to exit as soon
        // as child_port_ is set, even if other messages are queued. This needs
        // to drain all messages, because the use of edge triggering (EV_CLEAR)
        // means that if more than one message is in the queue when kevent()
        // returns, no more notifications will be generated.
        while (!checked_in_) {
          // If a proper message is received from child_port_check_in(),
          // this will call HandleChildPortCheckIn().
          mach_msg_return_t mr =
              MachMessageServer::Run(&child_port_server,
                                     server_port_set,
                                     MACH_MSG_OPTION_NONE,
                                     MachMessageServer::kOneShot,
                                     MachMessageServer::kNonblocking,
                                     MachMessageServer::kReceiveLargeIgnore,
                                     MACH_MSG_TIMEOUT_NONE);
          if (mr == MACH_RCV_TIMED_OUT) {
            break;
          } else if (mr != MACH_MSG_SUCCESS) {
            MACH_LOG(ERROR, mr) << "MachMessageServer::Run";
            return MACH_PORT_NULL;
          }
        }
        break;
      }

      case EVFILT_WRITE:
        // The write pipe is ready to be written to, or it’s at EOF. The former
        // case is uninteresting, but a notification for this may be presented
        // because the write pipe will be ready to be written to, at the latest,
        // when the client reads its messages from the read side of the same
        // pipe. Ignore that case. Multiple notifications for that situation
        // will not be generated because edge triggering (EV_CLEAR) is used
        // above.
        DCHECK_EQ(implicit_cast<int>(event.ident), pipe_write);
        if (event.flags & EV_EOF) {
          // There are no readers attached to the write pipe. The client has
          // closed its side of the pipe. There can be one last shot at
          // receiving messages, in case the check-in message is delivered
          // out of order, after the EOF notification.
          blocking = false;
        }
        break;

      default:
        NOTREACHED();
        break;
    }
  }

  mach_port_t child_port = MACH_PORT_NULL;
  std::swap(child_port_, child_port);
  return child_port;
}

kern_return_t ChildPortHandshake::HandleChildPortCheckIn(
    child_port_server_t server,
    const child_port_token_t token,
    mach_port_t port,
    mach_msg_type_name_t right_type,
    const mach_msg_trailer_t* trailer,
    bool* destroy_complex_request) {
  DCHECK_EQ(child_port_, kMachPortNull);

  if (token != token_) {
    // If the token’s not correct, someone’s attempting to spoof the legitimate
    // client.
    LOG(WARNING) << "ignoring incorrect token";
    *destroy_complex_request = true;
  } else {
    checked_in_ = true;

    if (right_type == MACH_MSG_TYPE_PORT_RECEIVE) {
      // The message needs to carry a send right or a send-once right. This
      // isn’t a strict requirement of the protocol, but users of this class
      // expect a send right or a send-once right, both of which can be managed
      // by base::mac::ScopedMachSendRight. It is invalid to store a receive
      // right in that scoper.
      LOG(WARNING) << "ignoring MACH_MSG_TYPE_PORT_RECEIVE";
      *destroy_complex_request = true;
    } else {
      // Communicate the child port back to the RunServer().
      // *destroy_complex_request is left at false, because RunServer() needs
      // the right to remain intact. It gives ownership of the right to its
      // caller.
      child_port_ = port;
    }
  }

  // This is a MIG simpleroutine, there is no reply message.
  return MIG_NO_REPLY;
}

// static
void ChildPortHandshake::RunClient(int pipe_read,
                                   mach_port_t port,
                                   mach_msg_type_name_t right_type) {
  base::ScopedFD pipe_read_owner(pipe_read);

  // Read the token and the service name from the read side of the pipe.
  child_port_token_t token;
  std::string service_name;
  RunClientInternal_ReadPipe(pipe_read, &token, &service_name);

  // Look up the server and check in with it by providing the token and port.
  RunClientInternal_SendCheckIn(service_name, token, port, right_type);
}

// static
void ChildPortHandshake::RunClientInternal_ReadPipe(int pipe_read,
                                                    child_port_token_t* token,
                                                    std::string* service_name) {
  // Read the token from the pipe.
  CheckedReadFD(pipe_read, token, sizeof(*token));

  // Read the service name from the pipe.
  uint32_t service_name_length;
  CheckedReadFD(pipe_read, &service_name_length, sizeof(service_name_length));
  DCHECK_LT(service_name_length,
            implicit_cast<uint32_t>(BOOTSTRAP_MAX_NAME_LEN));

  if (service_name_length > 0) {
    service_name->resize(service_name_length);
    CheckedReadFD(pipe_read, &(*service_name)[0], service_name_length);
  }
}

// static
void ChildPortHandshake::RunClientInternal_SendCheckIn(
    const std::string& service_name,
    child_port_token_t token,
    mach_port_t port,
    mach_msg_type_name_t right_type) {
  // Get a send right to the server by looking up the service with the bootstrap
  // server by name.
  mach_port_t server_port;
  kern_return_t kr =
      bootstrap_look_up(bootstrap_port, service_name.c_str(), &server_port);
  BOOTSTRAP_CHECK(kr == BOOTSTRAP_SUCCESS, kr) << "bootstrap_look_up";
  base::mac::ScopedMachSendRight server_port_owner(server_port);

  // Check in with the server.
  kr = child_port_check_in(server_port, token, port, right_type);
  MACH_CHECK(kr == KERN_SUCCESS, kr) << "child_port_check_in";
}

}  // namespace crashpad
