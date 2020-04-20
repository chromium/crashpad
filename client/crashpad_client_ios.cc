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

#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/mac/mach_logging.h"
#include "base/strings/stringprintf.h"
#include "client/client_argv_handling.h"
#include "snapshot/ios/process_snapshot_ios.h"
#include "util/ios/exception_processor.h"
#include "util/ios/ios_system_data_collector.h"
#include "util/mach/mach_message.h"
#include "util/mach/mach_message_server.h"
#include "util/mach/notify_server.h"
#include "util/posix/signals.h"

#pragma mark Structures
#pragma pack(push, 4)
typedef struct {
  mach_msg_header_t head;
  /* start of the kernel processed data */
  mach_msg_body_t msgh_body;
  mach_msg_port_descriptor_t thread;
  mach_msg_port_descriptor_t task;
  /* end of the kernel processed data */
  NDR_record_t NDR;
  exception_type_t exception;
  mach_msg_type_number_t codeCnt;
  mach_exception_data_type_t code[EXCEPTION_CODE_MAX];
  mach_msg_trailer_t trailer;
} MachExceptionMessage;

typedef struct {
  mach_msg_header_t head;
  NDR_record_t NDR;
  kern_return_t retCode;
} MachExceptionReply;
#pragma pack(pop)

namespace crashpad {

namespace {

// A base class for Crashpad signal handler implementations.
class SignalHandler {
 public:
  // Returns the currently installed signal hander.
  static SignalHandler* Get() {
    static SignalHandler* instance = new SignalHandler();
    return instance;
  }

  void SetupMachExcServer() {
    mach_port_t task = mach_task_self();
    port_ = MACH_PORT_NULL;
    kern_return_t kr =
        mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE, &port_);
    MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr) << "mach_port_deallocate";

    kr = mach_port_insert_right(task, port_, port_, MACH_MSG_TYPE_MAKE_SEND);
    MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr) << "mach_port_insert_right";

    exception_mask_t mask = EXC_MASK_BAD_ACCESS | EXC_MASK_BAD_INSTRUCTION |
                            EXC_MASK_ARITHMETIC | EXC_MASK_BREAKPOINT |
                            EXC_MASK_GUARD;

    kr = task_set_exception_ports(
        task,
        mask,
        port_,
        (int)(EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES),
        THREAD_STATE_NONE);
    MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr) << "task_set_exception_ports";

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
  }

  static void* ExceptionServer(void* argument) {
    SignalHandler* self = reinterpret_cast<SignalHandler*>(argument);
    pthread_setname_np("SignalHandler");

    mach_msg_return_t mr;

    do {
      MachExceptionMessage message;

      // Read message.
      memset(&message, 0, sizeof(MachExceptionMessage));
      mr = mach_msg(&message.head,
                    MACH_RCV_MSG | MACH_RCV_LARGE,
                    0,
                    sizeof(MachExceptionMessage),
                    self->port_,
                    MACH_MSG_TIMEOUT_NONE,
                    MACH_PORT_NULL);
      if (mr != MACH_MSG_SUCCESS) {
        break;
      }

      // Handle exception.
      self->HandleException(&message);

      // and now, reply
      MachExceptionReply reply;

      reply.head.msgh_bits =
          MACH_MSGH_BITS(MACH_MSGH_BITS_REMOTE(message.head.msgh_bits), 0);
      reply.head.msgh_remote_port = message.head.msgh_remote_port;
      reply.head.msgh_size = (mach_msg_size_t)sizeof(MachExceptionReply);
      reply.head.msgh_local_port = MACH_PORT_NULL;
      reply.head.msgh_id = message.head.msgh_id + 100;
      reply.NDR = NDR_record;
      reply.retCode = KERN_SUCCESS;
      mr = mach_msg(&reply.head,
                    MACH_SEND_MSG,
                    reply.head.msgh_size,
                    0,
                    MACH_PORT_NULL,
                    MACH_MSG_TIMEOUT_NONE,
                    MACH_PORT_NULL);
      if (mr != MACH_MSG_SUCCESS) {
        break;
      }
    } while (self->port_ != MACH_PORT_NULL && mr == MACH_MSG_SUCCESS);

    return nullptr;
  }

  bool Install(const std::set<int>* unhandled_signals) {
    SetupMachExcServer();
    return true;
    //    return Signals::InstallCrashHandlers(
    //        HandleSignal, 0, &old_actions_, unhandled_signals);
  }

  void HandleException(MachExceptionMessage* message) {
    ProcessSnapshotIOS process_snapshot;
    process_snapshot.Initialize(system_data);
    // Update exception ios to handle thread, exception, codes.
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
  mach_port_t port_;

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
