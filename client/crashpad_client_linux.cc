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

#include "client/crashpad_client.h"

#include "util/linux/exception_information.h"

namespace crashpad {

namespace {

// TODO copy pasted from crashpad_client_mac.cc. Factor out?
std::string FormatArgumentString(const std::string& name,
                                 const std::string& value) {
  return base::StringPrintf("--%s=%s", name.c_str(), value.c_str());
}

std::string FormatArgumentInt(const std::string& name, int value) {
  return base::StringPrintf("--%s=%d", name.c_str(), value);
}

auto g_handler_argv_strings = new std::vector<std::string>();
auto g_handler_argv = new std::vector<const char*>();

ExceptionInformation g_exception_information;
int g_handler_socket = kInvalidFileHandle;

// Launches a single use handler to snapshot this process.
void LaunchHandlerForCrash(int signo, siginfo_t* siginfo, void* context) {
  g_exception_information.siginfo_address =
      FromPointerCast<decltype(g_exception_information.siginfo_address)>(siginfo);
  g_exception_information.context_address =
      FromPointerCast<decltype(g_exception_information.context_address)>(context);
  g_exception_information.thread_id = gettid();

  pid_t pid = fork();
  if (pid < 0) {
    return;
  }
  if (pid == 0) {
    execv(g_handler_argv[0], &g_handler_argv[0]);
    return;
  }

  int status;
  waitpid(pid, &status, 0);
}

// Writes to the handler socket to indicate a crash and forks a ptrace broker to
// trace us for the handler.
void ForkBrokerForCrash(int signo, siginfo_t* siginfo, void* context) {
  g_exception_information.siginfo_address =
      FromPointerCast<decltype(g_exception_information.siginfo_address)>(siginfo);
  g_exception_information.context_address =
      FromPointerCast<decltype(g_exception_information.context_address)>(context);
  g_exception_information.thread_id = gettid();

  char c;
  if (!LoggingWriteFile(g_handler_socket, &c, sizeof(c))) {
    return;
  }

  pid_t pid = fork();
  if (pid < 0) {
    return;
  }
  if (pid == 0) {
    PtraceBroker broker;
    broker.Run();
    return;
  }

  int status;
  waitpid(pid, &status, 0);
}

// Configures a newly launched handler process via a proxy process.
void SetPtracerForCrash(int signo, siginfo_t* siginfo, void* context) {
  g_exception_information.siginfo_address =
      FromPointerCast<decltype(g_exception_information.siginfo_address)>(siginfo);
  g_exception_information.context_address =
      FromPointerCast<decltype(g_exception_information.context_address)>(context);
  g_exception_information.thread_id = gettid();

  char c;
  if (!LoggingWriteFile(g_handler_socket, &c, sizeof(c))) {
    return;
  }

  pid_t handler_pid;
  if (!LoggingReadFileExactly(g_handler_socket, &handler_pid, sizeof(handler_pid))) {
    return;
  }

  if (prctl(PR_SET_PTRACER, handler_pid) != 0) {
    return;
  }

  LoggingWriteFile(g_handler_socket, &c, sizeof(c));
  LoggingReadFile(g_handler_socket, &c, sizeof(c));
}

// Just let the handler know we're crashing.
void SignalHandlerForCrash(int signo, siginfo_t* siginfo, void* context) {
  g_exception_information.siginfo_address =
      FromPointerCast<decltype(g_exception_information.siginfo_address)>(siginfo);
  g_exception_information.context_address =
      FromPointerCast<decltype(g_exception_information.context_address)>(context);
  g_exception_information.thread_id = gettid();

  char c;
  LoggingWriteFile(g_handler_socket, &c, sizeof(c));
  LoggingReadFile(g_handler_socket, &c, sizeof(c));
}

void SetupHandlerArgv(
    const base::FilePath& handler,
    const base::FilePath& database,
    const base::FilePath& metrics_dir,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    const std::vector<std::string>& arguments) {
  g_handler_argv_strings->push_back(handler.value());
  for (const auto& argument : arguments) {
    g_handler_argv_strings->push_back(argument);
  }
#define PUSH_ARG(name, value) \
  do { \
    if (!value.empty()) { \
      g_handler_argv_strings->push_back(FormatArgumentString(name, value)); \
    } \
  } while (0);

  PUSH_ARG("database", database.value());
  PUSH_ARG("metrics-dir", metrics_dir.value());
  PUSH_ARG("url", url);
  for (const auto& kv : annotations) {
    PUSH_ARG("annotation", kv.first + '=' + kv.second);
  }

  g_handler_argv->reserve(g_handler_argv_strings.size() + 1);
  for (const auto& arg : g_handler_argv_strings) {
    g_handler_argv->push_back(arg.c_str());
  }
  g_handler_argv->push_back(nullptr);
}

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
  SetupHandlerArgv(handler, database, metrics_dir, url, annotations, arguments);

  pid_t pid = fork();
  if (pid < 0) {
    return false;
  }
  if (pid == 0) {
    execv(g_handler_argv[0], &g_handler_argv[0]);
    exit(1);
  }

  // TODO register with handler
  Signals::InstallCrashHandlers(SignalHandlerForCrash, 0, nullptr);
  return true;
}

bool StartHandlerOnCrash(
      const base::FilePath& handler,
      const base::FilePath& database,
      const base::FilePath& metrics_dir,
      const std::string& url,
      const std::map<std::string, std::string>& annotations,
      const std::vector<std::string>& arguments) {
  SetupHandlerArgv(handler, database, metrics_dir, url, annotations, arguments);
  Signals::InstallCrashHandlers(LaunchHandlerForCrash, 0, nullptr);
  return true;
}

bool RegisterProcess(const RegistrationRequest& request,
                     int socket) {
  if (g_handler_socket == kInvalidFileHandle) {
    return false;
  }
  // TODO transfer socket ownership to handler
  return LoggingWriteFile(g_handler_socket, &request, sizeof(request));
}

bool RegisterSelf(int socket, HandlerMode handler_mode) {
  RegistrationRequest request;
  request.process_id = getpid();
  request.exception_information_address =
      FromPointerCast<decltype(request.exception_information_address)>(
          &g_exception_information);
  request.use_broker = handler_mode == HandlerMode::kBroker;

  switch (mode) {
    case HandlerMode::kSignalSocket:
      Signals::InstallCrashHandlers(SignalHandlerForCrash, 0, nullptr);
      break;
    case HandlerMode::kSetPtracer:
      Signals::InstallCrashHandlers(SetPtracerForCrash, 0, nullptr);
      break;
    case HandlerMode::kBroker:
      Signals::InstallCrashHandlers(ForkBrokerForCrash, 0, nullptr);
      break;
  }
  return true;
}

}  // namespace crashpad
