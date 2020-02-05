// Copyright 2018 The Crashpad Authors. All rights reserved.
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

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "util/posix/signals.h"
#include <unistd.h>
#include "client/client_argv_handling.h"

namespace crashpad {

namespace {

// A base class for Crashpad signal handler implementations.
class SignalHandler {
 public:
  // Returns the currently installed signal hander. May be `nullptr` if no
  // handler has been installed.
  static SignalHandler* Get() { return handler_; }

  // The base implementation for all signal handlers, suitable for calling
  // directly to simulate signal delivery.
  bool HandleCrash(int signo, siginfo_t* siginfo, void* context) {
    _exit(0);
    return true;
  }

  bool Install(const std::set<int>* unhandled_signals) {
    DCHECK(!handler_);
    handler_ = this;
    return Signals::InstallCrashHandlers(
        HandleOrReraiseSignal, 0, &old_actions_, unhandled_signals);
  }

 protected:
  SignalHandler() = default;

 private:
  // The signal handler installed at OS-level.
  static void HandleOrReraiseSignal(int signo,
                                    siginfo_t* siginfo,
                                    void* context) {
    if (handler_->HandleCrash(signo, siginfo, context)) {
      return;
    }
    Signals::RestoreHandlerAndReraiseSignalOnReturn(
        siginfo, handler_->old_actions_.ActionForSignal(signo));
  }

  Signals::OldActions old_actions_ = {};
  static SignalHandler* handler_;
  DISALLOW_COPY_AND_ASSIGN(SignalHandler);
};
SignalHandler* SignalHandler::handler_ = nullptr;

}  // namespace

CrashpadClient::CrashpadClient() {}

CrashpadClient::~CrashpadClient() {}

bool CrashpadClient::StartHandler(
    const base::FilePath& handler,
    const base::FilePath& database,
    const base::FilePath& metrics_dir,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    const std::vector<std::string>& arguments,
    bool restartable,
    bool asynchronous_start) {
  DCHECK(!asynchronous_start);
  DCHECK(!restartable);

  auto signal_handler = SignalHandler::Get();
  return signal_handler->Install({});
}

}  // namespace crashpad
