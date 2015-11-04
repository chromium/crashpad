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

#include <signal.h>

#include "base/auto_reset.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/scoped_generic.h"
#include "base/mac/mach_logging.h"
#include "util/mach/composite_mach_message_server.h"
#include "util/mach/mach_extensions.h"
#include "util/mach/mach_message.h"
#include "util/mach/mach_message_server.h"
#include "util/mach/notify_server.h"

namespace crashpad {

namespace {

struct ResetSIGTERMTraits {
  static struct sigaction* InvalidValue() {
    return nullptr;
  }

  static void Free(struct sigaction* sa) {
    int rv = sigaction(SIGTERM, sa, nullptr);
    PLOG_IF(ERROR, rv != 0) << "sigaction";
  }
};
using ScopedResetSIGTERM =
    base::ScopedGeneric<struct sigaction*, ResetSIGTERMTraits>;

mach_port_t g_signal_notify_port;

// This signal handler is only operative when being run from launchd. It causes
// the exception handler server to stop running by sending it a synthesized
// no-senders notification.
void HandleSIGTERM(int sig, siginfo_t* siginfo, void* context) {
  DCHECK(g_signal_notify_port);

  // mach_no_senders_notification_t defines the receive side of this structure,
  // with a trailer element that’s undesirable for the send side.
  struct {
    mach_msg_header_t header;
    NDR_record_t ndr;
    mach_msg_type_number_t mscount;
  } no_senders_notification = {};
  no_senders_notification.header.msgh_bits =
      MACH_MSGH_BITS(MACH_MSG_TYPE_MAKE_SEND_ONCE, 0);
  no_senders_notification.header.msgh_size = sizeof(no_senders_notification);
  no_senders_notification.header.msgh_remote_port = g_signal_notify_port;
  no_senders_notification.header.msgh_local_port = MACH_PORT_NULL;
  no_senders_notification.header.msgh_id = MACH_NOTIFY_NO_SENDERS;
  no_senders_notification.ndr = NDR_record;
  no_senders_notification.mscount = 0;

  kern_return_t kr = mach_msg(&no_senders_notification.header,
                              MACH_SEND_MSG,
                              sizeof(no_senders_notification),
                              0,
                              MACH_PORT_NULL,
                              MACH_MSG_TIMEOUT_NONE,
                              MACH_PORT_NULL);
  MACH_CHECK(kr == KERN_SUCCESS, kr) << "mach_msg";
}

class ExceptionHandlerServerRun : public UniversalMachExcServer::Interface,
                                  public NotifyServer::DefaultInterface {
 public:
  ExceptionHandlerServerRun(
      mach_port_t exception_port,
      bool launchd,
      UniversalMachExcServer::Interface* exception_interface)
      : UniversalMachExcServer::Interface(),
        NotifyServer::DefaultInterface(),
        mach_exc_server_(this),
        notify_server_(this),
        composite_mach_message_server_(),
        exception_interface_(exception_interface),
        exception_port_(exception_port),
        notify_port_(NewMachPort(MACH_PORT_RIGHT_RECEIVE)),
        running_(true),
        launchd_(launchd) {
    CHECK(notify_port_.is_valid());

    composite_mach_message_server_.AddHandler(&mach_exc_server_);
    composite_mach_message_server_.AddHandler(&notify_server_);
  }

  ~ExceptionHandlerServerRun() {
  }

  void Run() {
    DCHECK(running_);

    kern_return_t kr;
    scoped_ptr<base::AutoReset<mach_port_t>> reset_signal_notify_port;
    struct sigaction old_sa;
    ScopedResetSIGTERM reset_sigterm;
    if (!launchd_) {
      // Request that a no-senders notification for exception_port_ be sent to
      // notify_port_.
      mach_port_t previous;
      kr = mach_port_request_notification(mach_task_self(),
                                          exception_port_,
                                          MACH_NOTIFY_NO_SENDERS,
                                          0,
                                          notify_port_.get(),
                                          MACH_MSG_TYPE_MAKE_SEND_ONCE,
                                          &previous);
      MACH_CHECK(kr == KERN_SUCCESS, kr) << "mach_port_request_notification";

      if (previous != MACH_PORT_NULL) {
        kr = mach_port_deallocate(mach_task_self(), previous);
        MACH_CHECK(kr == KERN_SUCCESS, kr) << "mach_port_deallocate";
      }
    } else {
      // A real no-senders notification would never be triggered, because
      // launchd maintains a send right to the service. When launchd wants the
      // job to exit, it will send a SIGTERM. See launchd.plist(5).
      //
      // Set up a SIGTERM handler that will cause Run() to return (incidentally,
      // by sending a synthetic no-senders notification).
      struct sigaction sa = {};
      sigemptyset(&sa.sa_mask);
      sa.sa_flags = SA_SIGINFO;
      sa.sa_sigaction = HandleSIGTERM;
      int rv = sigaction(SIGTERM, &sa, &old_sa);
      PCHECK(rv == 0) << "sigaction";
      reset_sigterm.reset(&old_sa);

      DCHECK(!g_signal_notify_port);
      reset_signal_notify_port.reset(new base::AutoReset<mach_port_t>(
          &g_signal_notify_port, notify_port_.get()));
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
    CHECK(server_port_set.is_valid());

    kr = mach_port_insert_member(
        mach_task_self(), exception_port_, server_port_set.get());
    MACH_CHECK(kr == KERN_SUCCESS, kr) << "mach_port_insert_member";

    kr = mach_port_insert_member(
        mach_task_self(), notify_port_.get(), server_port_set.get());
    MACH_CHECK(kr == KERN_SUCCESS, kr) << "mach_port_insert_member";

    // Run the server in kOneShot mode so that running_ can be reevaluated after
    // each message. Receipt of a valid no-senders notification causes it to be
    // set to false.
    while (running_) {
      // This will result in a call to CatchMachException() or
      // DoMachNotifyNoSenders() as appropriate.
      mach_msg_return_t mr =
          MachMessageServer::Run(&composite_mach_message_server_,
                                 server_port_set.get(),
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
                                   ConstThreadState old_state,
                                   mach_msg_type_number_t old_state_count,
                                   thread_state_t new_state,
                                   mach_msg_type_number_t* new_state_count,
                                   const mach_msg_trailer_t* trailer,
                                   bool* destroy_complex_request) override {
    if (exception_port != exception_port_) {
      LOG(WARNING) << "exception port mismatch";
      return KERN_FAILURE;
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

  // NotifyServer::DefaultInterface:

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
      return KERN_FAILURE;
    }

    running_ = false;

    return KERN_SUCCESS;
  }

 private:
  UniversalMachExcServer mach_exc_server_;
  NotifyServer notify_server_;
  CompositeMachMessageServer composite_mach_message_server_;
  UniversalMachExcServer::Interface* exception_interface_;  // weak
  mach_port_t exception_port_;  // weak
  base::mac::ScopedMachReceiveRight notify_port_;
  bool running_;
  bool launchd_;

  DISALLOW_COPY_AND_ASSIGN(ExceptionHandlerServerRun);
};

}  // namespace

ExceptionHandlerServer::ExceptionHandlerServer(
    base::mac::ScopedMachReceiveRight receive_port,
    bool launchd)
    : receive_port_(receive_port.Pass()),
      launchd_(launchd) {
  CHECK(receive_port_.is_valid());
}

ExceptionHandlerServer::~ExceptionHandlerServer() {
}

void ExceptionHandlerServer::Run(
    UniversalMachExcServer::Interface* exception_interface) {
  ExceptionHandlerServerRun run(
      receive_port_.get(), launchd_, exception_interface);
  run.Run();
}

}  // namespace crashpad
