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

namespace crashpad {

constexpr uint64_t kShutdown = 1;

struct ExceptionHandlerServer::Event {
  enum class Channel { kControl, kClient } channel;

  ScopedFileHandle fd;

  union {
    Registration registration;
  };
};

ExceptionHandlerServer::ExceptionHandlerServer()
    : events_(), delegate_(nullptr), pollfd_(), ready_(0), ok_to_run_(true) {}

ExceptionHandlerServer::~ExceptionHandlerServer() = default;

bool ExceptionHandlerServer::InitializeWithClient(
    const Registration& registration,
    ScopedFileHandle sock) {
  pollfd_.reset(epoll_create1(EPOLL_CLOEXEC));
  if (!pollfd_.is_valid()) {
    PLOG(ERROR) << "epoll_create1";
    return false;
  }

  ScopedFileHandle control(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK));
  if (!control.is_valid()) {
    return false;
  }
  controller_fd_ = control.get();

  if (!InstallController(std::move(control))) {
    return false;
  }

  return InstallClientSocket(registration, std::move(sock));
}

void ExceptionHandlerServer::Run(Delegate* delegate) {
  delegate_ = delegate;
  ready_.Signal();

  while (ok_to_run_ && events_.size() > 1) {
    epoll_event poll_event;
    int res = HANDLE_EINTR(epoll_wait(pollfd_.get(), &poll_event, 1, -1));
    if (res < 0) {
      PLOG(ERROR) << "epoll_wait";
      return;
    }

    HandleEvent(reinterpret_cast<Event*>(poll_event.data.ptr),
                poll_event.events);
  }
}

bool ExceptionHandlerServer::WaitUntilReady(double seconds) {
  return ready_.TimedWait(seconds);
}

void ExceptionHandlerServer::Stop() {
  LOG(INFO) << "Stopping server";
  ok_to_run_ = false;
  LoggingWriteFile(controller_fd_, &kShutdown, sizeof(kShutdown));
}

bool ExceptionHandlerServer::InstallEvent(std::unique_ptr<Event> event) {
  Event* eventp = event.get();

  LOG(INFO) << "installing event descriptor " << eventp->fd.get()
            << " with channel " << static_cast<int>(eventp->channel);

  if (!events_.insert(std::make_pair(event->fd.get(), std::move(event)))
           .second) {
    LOG(ERROR) << "duplicate descriptor";
    return false;
  }

  epoll_event poll_event;
  poll_event.events = EPOLLIN | EPOLLRDHUP | EPOLLPRI;
  poll_event.data.ptr = eventp;

  if (epoll_ctl(pollfd_.get(), EPOLL_CTL_ADD, eventp->fd.get(), &poll_event) !=
      0) {
    PLOG(ERROR) << "epoll_ctl";
    return false;
  }

  return true;
}

bool ExceptionHandlerServer::UninstallEvent(Event* event) {
  LOG(INFO) << "uninstalling event descriptor " << event->fd.get();
  if (epoll_ctl(pollfd_.get(), EPOLL_CTL_DEL, event->fd.get(), nullptr) != 0) {
    PLOG(ERROR) << "epoll_ctl";
    return false;
  }

  if (events_.erase(event->fd.get()) != 1) {
    LOG(ERROR) << "event not found";
    return false;
  }
  return true;
}

bool ExceptionHandlerServer::InstallController(ScopedFileHandle eventfd) {
  auto event = std::make_unique<Event>();
  event->channel = Event::Channel::kControl;
  event->fd.reset(eventfd.release());
  return InstallEvent(std::move(event));
}

bool ExceptionHandlerServer::InstallClientSocket(
    const Registration& registration,
    ScopedFileHandle socket) {
  auto event = std::make_unique<Event>();
  event->channel = Event::Channel::kClient;
  event->fd.reset(socket.release());
  event->registration = registration;
  LOG(INFO) << "Installing registration for "
            << event->registration.client_process_id << " at "
            << event->registration.exception_information_address;
  return InstallEvent(std::move(event));
}

void ExceptionHandlerServer::HandleEvent(Event* event, uint32_t event_type) {
  LOG(INFO) << "handling event for descriptor " << event->fd.get();

  if (event_type & EPOLLIN || event_type & EPOLLPRI) {
    switch (event->channel) {
      case Event::Channel::kControl:
        ReceiveControlMessage(event);
        return;

      case Event::Channel::kClient:
        ReceiveClientMessage(event);
        return;
    }
  }

  if (event_type & EPOLLERR || event_type & EPOLLHUP ||
      event_type & EPOLLRDHUP) {
    LOG(ERROR) << "Lost a connection ";
    UninstallEvent(event);
    return;
  }

  LOG(ERROR) << "Unexpected event " << event_type;
  return;
}

void ExceptionHandlerServer::ReceiveControlMessage(Event* event) {
  uint64_t eventno;
  if (!LoggingReadFileExactly(event->fd.get(), &eventno, sizeof(eventno))) {
    return;
  }
  if (eventno == kShutdown) {
    ok_to_run_ = false;
    return;
  }
  LOG(ERROR) << "unknown event number " << eventno;
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

  LOG(INFO) << "Receiving message from client " << event->fd.get();
  int res = recvmsg(event->fd.get(), &msg, 0);
  if (res < 0) {
    PLOG(ERROR) << "recvmsg";
    return;
  }
  if (res == 0) {
    LOG(ERROR) << "client shutdown";
    UninstallEvent(event);
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
    LOG(INFO) << "Registering client";

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
    bool success = InstallClientSocket(client_msg->registration,
                                       std::move(reg_socket));

    LoggingWriteFile(event->fd.get(), &success, sizeof(success));

    return;
  }

  if (client_msg->type == ClientToServerMessage::kCrashDumpRequest) {
    LOG(INFO) << "Client is crashing";
    delegate_->HandleException(event->registration.client_process_id,
                               event->registration.exception_information_address,
                               event->registration.use_broker);
  }

  LOG(INFO) << "Unknown message type";
  return;
}

}  // namespace crashpad
