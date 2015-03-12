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

#include "handler/mac/exception_handler_server.h"

#include "base/logging.h"
#include "base/mac/mach_logging.h"
#include "util/mach/composite_mach_message_server.h"
#include "util/mach/mach_extensions.h"
#include "util/mach/mach_message.h"
#include "util/mach/mach_message_server.h"
#include "util/mach/notify_server.h"

namespace crashpad {

namespace {

class ExceptionHandlerServerRun : public UniversalMachExcServer::Interface,
                                  public NotifyServer::Interface {
 public:
  ExceptionHandlerServerRun(
      mach_port_t exception_port,
      UniversalMachExcServer::Interface* exception_interface)
      : UniversalMachExcServer::Interface(),
        NotifyServer::Interface(),
        mach_exc_server_(this),
        notify_server_(this),
        composite_mach_message_server_(),
        exception_interface_(exception_interface),
        exception_port_(exception_port),
        notify_port_(NewMachPort(MACH_PORT_RIGHT_RECEIVE)),
        running_(true) {
    CHECK_NE(notify_port_, kMachPortNull);

    composite_mach_message_server_.AddHandler(&mach_exc_server_);
    composite_mach_message_server_.AddHandler(&notify_server_);
  }

  ~ExceptionHandlerServerRun() {
  }

  void Run() {
    DCHECK(running_);

    // Request that a no-senders notification for exception_port_ be sent to
    // notify_port_.
    mach_port_t previous;
    kern_return_t kr =
        mach_port_request_notification(mach_task_self(),
                                       exception_port_,
                                       MACH_NOTIFY_NO_SENDERS,
                                       0,
                                       notify_port_,
                                       MACH_MSG_TYPE_MAKE_SEND_ONCE,
                                       &previous);
    MACH_CHECK(kr == KERN_SUCCESS, kr) << "mach_port_request_notification";

    if (previous != MACH_PORT_NULL) {
      kr = mach_port_deallocate(mach_task_self(), previous);
      MACH_CHECK(kr == KERN_SUCCESS, kr) << "mach_port_deallocate";
    }

    // A single CompositeMachMessageServer will dispatch both exception messages
    // and the no-senders notification. Put both receive rights into a port set.
    //
    // A single receive right can’t be used because the notification request
    // requires a send-once right, which would prevent the no-senders condition
    // from ever existing. Using distinct receive rights also allows the handler
    // methods to ensure that the messages they process were sent by a holder of
    // the proper send right.
    base::mac::ScopedMachPortSet server_port_set(
        NewMachPort(MACH_PORT_RIGHT_PORT_SET));

    kr = mach_port_insert_member(
        mach_task_self(), exception_port_, server_port_set);
    MACH_CHECK(kr == KERN_SUCCESS, kr) << "mach_port_insert_member";

    kr = mach_port_insert_member(
        mach_task_self(), notify_port_, server_port_set);
    MACH_CHECK(kr == KERN_SUCCESS, kr) << "mach_port_insert_member";

    // Run the server in kOneShot mode so that running_ can be reevaluated after
    // each message. Receipt of a valid no-senders notification causes it to be
    // set to false.
    while (running_) {
      // This will result in a call to CatchMachException() or
      // DoMachNotifyNoSenders() as appropriate.
      mach_msg_return_t mr =
          MachMessageServer::Run(&composite_mach_message_server_,
                                 server_port_set,
                                 kMachMessageReceiveAuditTrailer,
                                 MachMessageServer::kOneShot,
                                 MachMessageServer::kReceiveLargeIgnore,
                                 kMachMessageTimeoutWaitIndefinitely);
      MACH_CHECK(mr == MACH_MSG_SUCCESS, mr) << "MachMessageServer::Run";
    }
  }

  // UniversalMachExcServer::Interface:

  kern_return_t CatchMachException(exception_behavior_t behavior,
                                   exception_handler_t exception_port,
                                   thread_t thread,
                                   task_t task,
                                   exception_type_t exception,
                                   const mach_exception_data_type_t* code,
                                   mach_msg_type_number_t code_count,
                                   thread_state_flavor_t* flavor,
                                   const natural_t* old_state,
                                   mach_msg_type_number_t old_state_count,
                                   thread_state_t new_state,
                                   mach_msg_type_number_t* new_state_count,
                                   const mach_msg_trailer_t* trailer,
                                   bool* destroy_complex_request) override {
    if (exception_port != exception_port_) {
      LOG(WARNING) << "exception port mismatch";
      return MIG_BAD_ID;
    }

    return exception_interface_->CatchMachException(behavior,
                                                    exception_port,
                                                    thread,
                                                    task,
                                                    exception,
                                                    code,
                                                    code_count,
                                                    flavor,
                                                    old_state,
                                                    old_state_count,
                                                    new_state,
                                                    new_state_count,
                                                    trailer,
                                                    destroy_complex_request);
  }

  // NotifyServer::Interface:

  kern_return_t DoMachNotifyPortDeleted(
      notify_port_t notify,
      mach_port_name_t name,
      const mach_msg_trailer_t* trailer) override {
    return UnimplementedNotifyRoutine(notify);
  }

  kern_return_t DoMachNotifyPortDestroyed(notify_port_t notify,
                                          mach_port_t rights,
                                          const mach_msg_trailer_t* trailer,
                                          bool* destroy_request) override {
    *destroy_request = true;
    return UnimplementedNotifyRoutine(notify);
  }

  kern_return_t DoMachNotifyNoSenders(
      notify_port_t notify,
      mach_port_mscount_t mscount,
      const mach_msg_trailer_t* trailer) override {
    if (notify != notify_port_) {
      // The message was received as part of a port set. This check ensures that
      // only the authorized sender of the no-senders notification is able to
      // stop the exception server. Otherwise, a malicious client would be able
      // to craft and send a no-senders notification via its exception port, and
      // cause the handler to stop processing exceptions and exit.
      LOG(WARNING) << "notify port mismatch";
      return MIG_BAD_ID;
    }

    running_ = false;

    return KERN_SUCCESS;
  }

  kern_return_t DoMachNotifySendOnce(
      notify_port_t notify,
      const mach_msg_trailer_t* trailer) override {
    return UnimplementedNotifyRoutine(notify);
  }

  kern_return_t DoMachNotifyDeadName(
      notify_port_t notify,
      mach_port_name_t name,
      const mach_msg_trailer_t* trailer) override {
    return UnimplementedNotifyRoutine(notify);
  }

 private:
  kern_return_t UnimplementedNotifyRoutine(notify_port_t notify) {
    // Most of the routines in the notify subsystem are not expected to be
    // called.
    if (notify != notify_port_) {
      LOG(WARNING) << "notify port mismatch";
      return MIG_BAD_ID;
    }

    NOTREACHED();
    return KERN_FAILURE;
  }

  UniversalMachExcServer mach_exc_server_;
  NotifyServer notify_server_;
  CompositeMachMessageServer composite_mach_message_server_;
  UniversalMachExcServer::Interface* exception_interface_;  // weak
  mach_port_t exception_port_;  // weak
  base::mac::ScopedMachReceiveRight notify_port_;
  bool running_;

  DISALLOW_COPY_AND_ASSIGN(ExceptionHandlerServerRun);
};

}  // namespace

ExceptionHandlerServer::ExceptionHandlerServer()
    : receive_port_(NewMachPort(MACH_PORT_RIGHT_RECEIVE)) {
  CHECK_NE(receive_port_, kMachPortNull);
}

ExceptionHandlerServer::~ExceptionHandlerServer() {
}

void ExceptionHandlerServer::Run(
    UniversalMachExcServer::Interface* exception_interface) {
  ExceptionHandlerServerRun run(receive_port_, exception_interface);
  run.Run();
}

}  // namespace crashpad
