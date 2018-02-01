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

#include <fcntl.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "util/file/file_io.h"
#include "util/linux/exception_handler_client.h"
#include "util/linux/exception_information.h"
#include "util/misc/from_pointer_cast.h"
#include "util/posix/double_fork_and_exec.h"
#include "util/posix/signals.h"

namespace crashpad {

namespace {

std::string FormatArgumentString(const std::string& name,
                                 const std::string& value) {
  return base::StringPrintf("--%s=%s", name.c_str(), value.c_str());
}

std::string FormatArgumentInt(const std::string& name, int value) {
  return base::StringPrintf("--%s=%d", name.c_str(), value);
}

std::string FormatArgumentAddress(const std::string& name, void* addr) {
  return base::StringPrintf("--%s=%p", name.c_str(), addr);
}

void BuildHandlerArgvStrings(
    const base::FilePath& handler,
    const base::FilePath& database,
    const base::FilePath& metrics_dir,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    const std::vector<std::string>& arguments,
    std::vector<std::string>* argv_strings) {
  argv_strings->clear();

  argv_strings->push_back(handler.value());
  for (const auto& argument : arguments) {
    argv_strings->push_back(argument);
  }

  if (!database.empty()) {
    argv_strings->push_back(FormatArgumentString("database", database.value()));
  }

  if (!metrics_dir.empty()) {
    argv_strings->push_back(
        FormatArgumentString("metrics-dir", metrics_dir.value()));
  }

  if (!url.empty()) {
    argv_strings->push_back(FormatArgumentString("url", url));
  }

  for (const auto& kv : annotations) {
    argv_strings->push_back(
        FormatArgumentString("annotation", kv.first + '=' + kv.second));
  }
}

void ConvertArgvStrings(const std::vector<std::string> argv_strings,
                        std::vector<const char*>* argv) {
  argv->clear();
  argv->reserve(argv_strings.size() + 1);
  for (const auto& arg : argv_strings) {
    argv->push_back(arg.c_str());
  }
  argv->push_back(nullptr);
}

// Launches a single use handler to snapshot this process.
class LaunchAtCrashHandler {
 public:
  static LaunchAtCrashHandler* Get() {
    static LaunchAtCrashHandler* instance = new LaunchAtCrashHandler();
    return instance;
  }

  bool Initialize(std::vector<std::string>* argv_in) {
    argv_strings_.swap(*argv_in);

    argv_strings_.push_back(FormatArgumentAddress("trace-parent-with-exception",
                                                  &exception_information_));

    ConvertArgvStrings(argv_strings_, &argv_);
    return Signals::InstallCrashHandlers(HandleCrash, 0, nullptr);
  }

 private:
  LaunchAtCrashHandler() = default;

  ~LaunchAtCrashHandler() = delete;

  static void HandleCrash(int signo, siginfo_t* siginfo, void* context) {
    auto state = Get();
    auto exception_information = &state->exception_information_;

    exception_information->siginfo_address =
        FromPointerCast<decltype(exception_information->siginfo_address)>(
            siginfo);
    exception_information->context_address =
        FromPointerCast<decltype(exception_information->context_address)>(
            context);
    exception_information->thread_id = syscall(SYS_gettid);

    pid_t pid = fork();
    if (pid < 0) {
      return;
    }
    if (pid == 0) {
      execv(state->argv_[0], const_cast<char* const*>(state->argv_.data()));
      return;
    }

    int status;
    waitpid(pid, &status, 0);
  }

  std::vector<std::string> argv_strings_;
  std::vector<const char*> argv_;
  ExceptionInformation exception_information_;

  DISALLOW_COPY_AND_ASSIGN(LaunchAtCrashHandler);
};

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
  // TODO(jperaza): Implement this after the Android/Linux ExceptionHandlerSever
  // supports accepting new connections.
  // https://crashpad.chromium.org/bug/30
  NOTREACHED();
  return false;
}

bool CrashpadClient::StartHandlerAtCrash(
    const base::FilePath& handler,
    const base::FilePath& database,
    const base::FilePath& metrics_dir,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    const std::vector<std::string>& arguments) {
  std::vector<std::string> argv;
  BuildHandlerArgvStrings(
      handler, database, metrics_dir, url, annotations, arguments, &argv);

  auto signal_handler = LaunchAtCrashHandler::Get();
  return signal_handler->Initialize(&argv);
}

bool CrashpadClient::StartHandlerForClient(
    const base::FilePath& handler,
    const base::FilePath& database,
    const base::FilePath& metrics_dir,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    const std::vector<std::string>& arguments,
    int socket) {
  std::vector<std::string> argv;
  BuildHandlerArgvStrings(
      handler, database, metrics_dir, url, annotations, arguments, &argv);

  argv.push_back(FormatArgumentInt("initial-client", socket));

  return DoubleForkAndExec(argv, socket, true, nullptr);
}

}  // namespace crashpad
