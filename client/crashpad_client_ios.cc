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
#include "util/thread/thread.h"

namespace crashpad {

namespace {

class MachExceptionServer;

// A base class for signal handler and mach exception server.
class CrashHandler {
 public:
  static CrashHandler* Get() {
    static CrashHandler* instance = new CrashHandler();
    return instance;
  }

  bool Install() {
    return Signals::InstallHandler(SIGABRT, CatchSignal, 0, &old_action_);
  }

  void HandleSignal(int signo, siginfo_t* siginfo, ucontext_t* context) {
    // TODO(justincohen): This is incomplete.
    ProcessSnapshotIOS process_snapshot;
    process_snapshot.Initialize(system_data);
    process_snapshot.SetException(siginfo, context);
  }

  void HandleException(exception_behavior_t behavior,
                       thread_t thread,
                       exception_type_t exception,
                       const mach_exception_data_type_t* code,
                       mach_msg_type_number_t code_count,
                       thread_state_flavor_t flavor,
                       ConstThreadState old_state,
                       mach_msg_type_number_t old_state_count) {
    // TODO(justincohen): This is incomplete.
    ProcessSnapshotIOS process_snapshot;
    process_snapshot.Initialize(system_data);
    process_snapshot.SetException(behavior,
                                  thread,
                                  exception,
                                  code,
                                  code_count,
                                  flavor,
                                  old_state,
                                  old_state_count);
  }

 private:
  CrashHandler();

  void HandleSignalAndReraiseSignal(int signo,
                                    siginfo_t* siginfo,
                                    void* context) {
    HandleSignal(signo, siginfo, reinterpret_cast<ucontext_t*>(context));
    // Always call system handler.
    Signals::RestoreHandlerAndReraiseSignalOnReturn(siginfo, &old_action_);
  }

  // The signal handler installed at OS-level.
  static void CatchSignal(int signo, siginfo_t* siginfo, void* context) {
    Get()->HandleSignalAndReraiseSignal(signo, siginfo, context);
  }

  struct sigaction old_action_;

  // Collect system data before a signal or exception handler is triggered.
  IOSSystemDataCollector system_data;
  std::unique_ptr<MachExceptionServer> mach_exception_server_;
  DISALLOW_COPY_AND_ASSIGN(CrashHandler);
};

// A universal mach server.
class MachExceptionServer : public Thread,
                            public UniversalMachExcServer::Interface {
 public:
  MachExceptionServer() { Start(); }

 private:
  void ThreadMain() {
    base::mac::ScopedMachReceiveRight port(
        NewMachPort(MACH_PORT_RIGHT_RECEIVE));
    CHECK(port.is_valid());

    kern_return_t kr = mach_port_insert_right(
        mach_task_self(), port.get(), port.get(), MACH_MSG_TYPE_MAKE_SEND);
    MACH_LOG_IF(ERROR, kr != KERN_SUCCESS, kr) << "mach_port_insert_right";

    ExceptionPorts exception_ports(ExceptionPorts::kTargetTypeTask, TASK_NULL);
    exception_ports.SetExceptionPort(
        EXC_MASK_BAD_ACCESS | EXC_MASK_BAD_INSTRUCTION |
            EXC_MASK_ARITHMETIC |  // TODO: put back ` EXC_MASK_BREAKPOINT |`
            EXC_MASK_GUARD,
        port.get(),
        EXCEPTION_STATE_IDENTITY | MACH_EXCEPTION_CODES,
        MACHINE_THREAD_STATE);

    UniversalMachExcServer universal_mach_exc_server(this);
    running_ = true;
    while (running_) {
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
    running_ = false;
    CrashHandler::Get()->HandleException(behavior,
                                         thread,
                                         exception,
                                         code,
                                         code_count,
                                         *flavor,
                                         old_state,
                                         old_state_count);
    ExcServerCopyState(
        behavior, old_state, old_state_count, new_state, new_state_count);
    return ExcServerSuccessfulReturnValue(exception, behavior, false);
  }

  bool running_;
  DISALLOW_COPY_AND_ASSIGN(MachExceptionServer);
};

CrashHandler::CrashHandler() {
  mach_exception_server_.reset(new MachExceptionServer());
}

}  // namespace

CrashpadClient::CrashpadClient() {}

CrashpadClient::~CrashpadClient() {}

bool CrashpadClient::StartCrashpadInProcessHandler() {
  InstallObjcExceptionPreprocessor();
  return CrashHandler::Get()->Install();
}

// static
void CrashpadClient::DumpWithoutCrash() {
  DCHECK(CrashHandler::Get());
  siginfo_t siginfo = {};
  CrashHandler::Get()->HandleSignal(siginfo.si_signo, &siginfo, nullptr);
}

}  // namespace crashpad
