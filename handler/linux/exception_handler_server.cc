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

#include "handler/linux/exception_handler_server.h"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <utility>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "util/file/file_io.h"
#include "util/misc/as_underlying_type.h"

namespace crashpad {

struct ExceptionHandlerServer::Event {
  enum class Type { kShutdown, kClientMessage } type;

  ScopedFileHandle fd;

  union {
    Registration registration;
  };
};

ExceptionHandlerServer::ExceptionHandlerServer()
    : clients_(),
      shutdown_event_(),
      delegate_(nullptr),
      pollfd_(),
      keep_running_(true) {}

ExceptionHandlerServer::~ExceptionHandlerServer() = default;

bool ExceptionHandlerServer::InitializeWithClient(
    const Registration& registration,
    ScopedFileHandle sock) {
  pollfd_.reset(epoll_create1(EPOLL_CLOEXEC));
  if (!pollfd_.is_valid()) {
    PLOG(ERROR) << "epoll_create1";
    return false;
  }

  shutdown_event_ = std::make_unique<Event>();
  shutdown_event_->type = Event::Type::kShutdown;
  shutdown_event_->fd.reset(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK));
  if (!shutdown_event_->fd.is_valid()) {
    PLOG(ERROR) << "eventfd";
    return false;
  }

  epoll_event poll_event;
  poll_event.events = EPOLLIN;
  poll_event.data.ptr = shutdown_event_.get();
  if (epoll_ctl(pollfd_.get(),
                EPOLL_CTL_ADD,
                shutdown_event_->fd.get(),
                &poll_event) != 0) {
    PLOG(ERROR) << "epoll_ctl";
    return false;
  }

  return InstallClientRegistration(registration, std::move(sock));
}

void ExceptionHandlerServer::Run(Delegate* delegate) {
  delegate_ = delegate;

  while (keep_running_ && clients_.size() > 0) {
    LOG(INFO) << "Waiting for clients " << clients_.size();
    epoll_event poll_event;
    int res = HANDLE_EINTR(epoll_wait(pollfd_.get(), &poll_event, 1, -1));
    if (res < 0) {
      PLOG(ERROR) << "epoll_wait";
      return;
    }

    Event* eventp = reinterpret_cast<Event*>(poll_event.data.ptr);
    if (eventp->type == Event::Type::kShutdown) {
      LOG(INFO) << "Received shutdown event";
    } else {
      HandleEvent(eventp, poll_event.events);
      LOG(INFO) << "event handled";
    }
  }
}

void ExceptionHandlerServer::Stop() {
  LOG(INFO) << "Stopping server";
  keep_running_ = false;
  if (shutdown_event_ && shutdown_event_->fd.is_valid()) {
    uint64_t value = 1;
    LoggingWriteFile(shutdown_event_->fd.get(), &value, sizeof(value));
  }
}

bool ExceptionHandlerServer::InstallClientRegistration(
    const Registration& registration,
    ScopedFileHandle socket) {
  auto event = std::make_unique<Event>();
  event->type = Event::Type::kClientMessage;
  event->fd.reset(socket.release());
  event->registration = registration;

  Event* eventp = event.get();

  if (!clients_.insert(std::make_pair(event->fd.get(), std::move(event)))
           .second) {
    LOG(ERROR) << "duplicate descriptor";
    return false;
  }

  epoll_event poll_event;
  poll_event.events = EPOLLIN | EPOLLRDHUP;
  poll_event.data.ptr = eventp;

  if (epoll_ctl(pollfd_.get(), EPOLL_CTL_ADD, eventp->fd.get(), &poll_event) !=
      0) {
    PLOG(ERROR) << "epoll_ctl";
    clients_.erase(eventp->fd.get());
    return false;
  }

  return true;
}

bool ExceptionHandlerServer::UninstallClientRegistration(Event* event) {
  LOG(INFO) << "Uninstalling client " << event->fd.get();
  if (epoll_ctl(pollfd_.get(), EPOLL_CTL_DEL, event->fd.get(), nullptr) != 0) {
    PLOG(ERROR) << "epoll_ctl";
    return false;
  }

  if (clients_.erase(event->fd.get()) != 1) {
    LOG(ERROR) << "event not found";
    return false;
  }
  return true;
}

void ExceptionHandlerServer::HandleEvent(Event* event, uint32_t event_type) {
  DCHECK_EQ(AsUnderlyingType(event->type),
            AsUnderlyingType(Event::Type::kClientMessage));

  if (event_type & EPOLLERR) {
    socklen_t errno_len = sizeof(errno);
    if (getsockopt(event->fd.get(), SOL_SOCKET, SO_ERROR, &errno, &errno_len) !=
        0) {
      PLOG(ERROR) << "getsockopt";
    } else {
      PLOG(ERROR) << "EPOLLERR";
    }
    UninstallClientRegistration(event);
    return;
  }

  if (event_type & EPOLLIN) {
    ReceiveClientMessage(event);
    LOG(INFO) << "Client message received";
    return;
  }

  if (event_type & EPOLLHUP || event_type & EPOLLRDHUP) {
    UninstallClientRegistration(event);
    return;
  }

  LOG(ERROR) << "Unexpected event " << event_type;
  return;
}

void ExceptionHandlerServer::ReceiveClientMessage(Event* event) {
  ClientToServerMessage message;
  memset(&message, 0, sizeof(message));

  iovec iov;
  iov.iov_base = &message;
  iov.iov_len = sizeof(message);

  msghdr msg;
  msg.msg_name = nullptr;
  msg.msg_namelen = 0;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  char cmsg_buf[CMSG_SPACE(sizeof(int))];
  msg.msg_control = cmsg_buf;
  msg.msg_controllen = sizeof(cmsg_buf);
  msg.msg_flags = 0;

  int res = recvmsg(event->fd.get(), &msg, 0);
  if (res < 0) {
    PLOG(ERROR) << "recvmsg";
    return;
  }
  if (res == 0) {
    LOG(ERROR) << "client shutdown";
    UninstallClientRegistration(event);
    LOG(INFO) << "Client uninstalled";
    return;
  }

  if (msg.msg_name != nullptr || msg.msg_namelen != 0) {
    LOG(ERROR) << "unexpected msg name";
    return;
  }

  if (msg.msg_iovlen != 1) {
    LOG(ERROR) << "unexpected iovlen";
    return;
  }

  if (msg.msg_iov[0].iov_len != sizeof(ClientToServerMessage)) {
    LOG(ERROR) << "unexpected message size " << msg.msg_iov[0].iov_len;
    return;
  }
  auto client_msg =
      reinterpret_cast<ClientToServerMessage*>(msg.msg_iov[0].iov_base);

  if (client_msg->type == ClientToServerMessage::kRegistration) {
    cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg == nullptr) {
      LOG(ERROR) << "missing registration descriptor";
      return;
    }

    if (cmsg->cmsg_level != SOL_SOCKET) {
      LOG(ERROR) << "unexpected cmsg_level " << cmsg->cmsg_level;
      return;
    }

    if (cmsg->cmsg_type != SCM_RIGHTS) {
      LOG(ERROR) << "unexpected cmsg_type " << cmsg->cmsg_type;
      return;
    }

    if (cmsg->cmsg_len != CMSG_LEN(sizeof(int))) {
      LOG(ERROR) << "unexpected cmsg_len " << cmsg->cmsg_len;
      return;
    }

    ScopedFileHandle reg_socket(*reinterpret_cast<int*>(CMSG_DATA(cmsg)));
    bool success = InstallClientRegistration(client_msg->registration,
                                             std::move(reg_socket));

    LoggingWriteFile(event->fd.get(), &success, sizeof(success));

    return;
  }

  if (client_msg->type == ClientToServerMessage::kCrashDumpRequest) {
    LOG(INFO) << "Client is crashing";
    delegate_->HandleException(event->registration, event->fd.get());
  }

  LOG(INFO) << "Unknown message type";
  return;
}

}  // namespace crashpad
