// Copyright 2020 The Crashpad Authors. All rights reserved.
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

#include "client/crashpad_client.h"

#include <unistd.h>

#include "base/logging.h"
#include "base/mac/mach_logging.h"
#include "base/mac/scoped_mach_port.h"
#include "base/strings/stringprintf.h"
#include "client/client_argv_handling.h"
#include "snapshot/ios/process_snapshot_ios.h"
#include "util/ios/exception_processor.h"
#include "util/ios/ios_system_data_collector.h"
#include "util/mach/exc_server_variants.h"
#include "util/mach/exception_ports.h"
#include "util/mach/mach_message.h"
#include "util/posix/signals.h"

namespace crashpad {

namespace {

// A base class for Crashpad signal handler implementations.
class SignalHandler : public UniversalMachExcServer::Interface {
 public:
  // Returns the currently installed signal hander.
  static SignalHandler* Get() {
    static SignalHandler* instance = new SignalHandler();
    return instance;
  }

  bool Install(const std::set<int>* unhandled_signals) {
    //    return Signals::InstallCrashHandlers(
    //        HandleSignal, 0, &old_actions_, unhandled_signals);
    pthread_attr_t attr;
    errno = pthread_attr_init(&attr);
    PLOG_IF(WARNING, errno != 0) << "pthread_attr_init";

    errno = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    PLOG_IF(WARNING, errno != 0) << "pthread_attr_setdetachstate";

    stack_ = valloc(256 * 1024);
    errno = pthread_attr_setstack(&attr, stack_, 256 * 1024);
    PLOG_IF(WARNING, errno != 0) << "pthread_attr_setstack";

    pthread_t pthread;
    errno = pthread_create(&pthread, &attr, ExceptionServer, this);
    PLOG_IF(WARNING, errno != 0) << "pthread_create";

    pthread_attr_destroy(&attr);
    return true;
  }

  static void* ExceptionServer(void* argument) {
    SignalHandler* self = reinterpret_cast<SignalHandler*>(argument);

    base::mac::ScopedMachReceiveRight port(
        NewMachPort(MACH_PORT_RIGHT_RECEIVE));
    CHECK(port.is_valid());

    kern_return_t kr = mach_port_insert_right(
        mach_task_self(), port.get(), port.get(), MACH_MSG_TYPE_MAKE_SEND);
    MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr) << "mach_port_insert_right";

    ExceptionPorts exception_ports(ExceptionPorts::kTargetTypeTask, TASK_NULL);
    exception_ports.SetExceptionPort(
        ExcMaskValid(),
        port.get(),
        EXCEPTION_STATE_IDENTITY | MACH_EXCEPTION_CODES,
        MACHINE_THREAD_STATE);

    UniversalMachExcServer universal_mach_exc_server(self);
    self->running_ = true;
    while (self->running_) {
      mach_msg_return_t mr =
          MachMessageServer::Run(&universal_mach_exc_server,
                                 port.get(),
                                 kMachMessageReceiveAuditTrailer,
                                 MachMessageServer::kOneShot,
                                 MachMessageServer::kReceiveLargeIgnore,
                                 kMachMessageTimeoutWaitIndefinitely);
      MACH_CHECK(mr == MACH_MSG_SUCCESS || mr == MACH_SEND_INVALID_DEST, mr)
          << "MachMessageServer::Run";
    }
    return nullptr;
  }

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
                                   bool* destroy_complex_request) {
    printf("HELLO WORLD!!\n");
    printf("HELLO WORLD!!\n");
    printf("HELLO WORLD!!\n");
    running_ = false;
    ExcServerCopyState(
        behavior, old_state, old_state_count, new_state, new_state_count);
    return ExcServerSuccessfulReturnValue(exception, behavior, false);
  }

  void HandleCrash(int signo, siginfo_t* siginfo, void* context) {
    // TODO(justincohen): This is incomplete.
    ProcessSnapshotIOS process_snapshot;
    process_snapshot.Initialize(system_data);
    process_snapshot.SetException(siginfo,
                                  reinterpret_cast<ucontext_t*>(context));
  }

 private:
  SignalHandler() = default;

  // The base implementation for all signal handlers, suitable for calling
  // directly to simulate signal delivery.
  void HandleCrashAndReraiseSignal(int signo,
                                   siginfo_t* siginfo,
                                   void* context) {
    HandleCrash(signo, siginfo, context);
    // Always call system handler.
    Signals::RestoreHandlerAndReraiseSignalOnReturn(
        siginfo, old_actions_.ActionForSignal(signo));
  }

  // The signal handler installed at OS-level.
  static void HandleSignal(int signo, siginfo_t* siginfo, void* context) {
    Get()->HandleCrashAndReraiseSignal(signo, siginfo, context);
  }

  Signals::OldActions old_actions_ = {};

  // Collect some system data before the signal handler is triggered.
  IOSSystemDataCollector system_data;

  void* stack_;
  bool running_;

  DISALLOW_COPY_AND_ASSIGN(SignalHandler);
};

}  // namespace

CrashpadClient::CrashpadClient() {}

CrashpadClient::~CrashpadClient() {}

bool CrashpadClient::StartCrashpadInProcessHandler() {
  InstallObjcExceptionPreprocessor();
  return SignalHandler::Get()->Install(nullptr);
}

// static
void CrashpadClient::DumpWithoutCrash() {
  DCHECK(SignalHandler::Get());
  siginfo_t siginfo = {};
  SignalHandler::Get()->HandleCrash(siginfo.si_signo, &siginfo, nullptr);
}

}  // namespace crashpad
