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
#include "base/strings/stringprintf.h"
#include "client/client_argv_handling.h"
#include "snapshot/ios/process_snapshot_ios.h"
#include "util/posix/signals.h"

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

  bool Install(const std::set<int>* unhandled_signals) {
    return Signals::InstallCrashHandlers(
        HandleSignal, 0, &old_actions_, unhandled_signals);
  }

  void HandleCrash(int signo, siginfo_t* siginfo, void* context) {
    // TODO(justincohen): This is incomplete.
    ProcessSnapshotIOS process_snapshot;
    process_snapshot.Initialize();
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

  DISALLOW_COPY_AND_ASSIGN(SignalHandler);
};

}  // namespace

CrashpadClient::CrashpadClient() {}

CrashpadClient::~CrashpadClient() {}

bool CrashpadClient::StartCrashpadInProcessHandler() {
  return SignalHandler::Get()->Install(nullptr);
}

// static
void CrashpadClient::DumpWithoutCrash() {
  DCHECK(SignalHandler::Get());

  siginfo_t siginfo = {};
  SignalHandler::Get()->HandleCrash(siginfo.si_signo, &siginfo, nullptr);
}
}  // namespace crashpad
