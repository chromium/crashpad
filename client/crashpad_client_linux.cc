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

#include <sys/socket.h>
#include <sys/types.h>

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

std::string FormatArgumentAddress(const std::string& name, void* addr) {
  return base::StringPrintf("--%s=%p", name.c_str(), addr);
}

auto g_handler_argv_strings = new std::vector<std::string>();
auto g_handler_argv = new std::vector<const char*>();

ExceptionInformation g_exception_information;
int g_handler_socket = kInvalidFileHandle;

// Launches a single use handler to snapshot this process.
void LaunchHandlerForCrash(int signo, siginfo_t* siginfo, void* context) {
  g_exception_information.siginfo_address =
      FromPointerCast<decltype(g_exception_information.siginfo_address)>(
          siginfo);
  g_exception_information.context_address =
      FromPointerCast<decltype(g_exception_information.context_address)>(
          context);
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

bool CompleteRegistrationByProxy(int handler_socket, PtraceMode mode) {
  RegistrationRequest request;
  request.process_id = getpid();
  request.exception_information_address =
      FromPointerCast<decltype(request.exception_information_address)>(
          &g_exception_information);
  request.use_broker = mode == PtraceMode::kBroker;
  if (!WriteFile(handler_socket, &request, sizeof(request))) {
    return false;
  }

  if (mode == PtraceMode::kSetPtracer) {
    pid_t handler_pid;
    if (!ReadFileExactly(handler_socket, &handler_pid, sizeof(handler_pid))) {
      return false;
    }
    return prctl(PR_SET_PTRACER, handler_pid) != 0;
  } else {
    bool success;
    return ReadFileExactly(handler_socket, &success, sizeof(success)) &&
           success;
  }
}

void ForkBrokerForCrash(int signo, siginfo_t* siginfo, void* context) {
  g_exception_information.siginfo_address =
      FromPointerCast<decltype(g_exception_information.siginfo_address)>(
          siginfo);
  g_exception_information.context_address =
      FromPointerCast<decltype(g_exception_information.context_address)>(
          context);
  g_exception_information.thread_id = gettid();

  if (g_register_at_crash &&
      !CompleteRegistrationByProxy(g_handler_socket, PtraceMode::kBroker)) {
    return;
  }

  if (!RequestCrashDump(g_handler_socket)) {
    return;
  }

  pid_t pid = fork();
  if (pid < 0) {
    return;
  }

  if (pid == 0) {
#if defined(ARCH_CPU_64_BITS)
    constexpr bool am_64_bit = true;
#else
    constexpr bool am_64_bit = false;
#endif  // ARCH_CPU_64_BITS
    PtraceBroker broker(g_handler_socket, am_64_bit);
    _exit(broker.Run() ? EXIT_SUCCESS : EXIT_FAILURE);
  }

  int status;
  waitpid(pid, &status, 0);
  if (status == EXIT_SUCCESS) {
    WaitForCrashDumpDone(g_handler_socket);
  }
}

void SignalHandlerForCrash(int signo, siginfo_t* siginfo, void* context) {
  g_exception_information.siginfo_address =
      FromPointerCast<decltype(g_exception_information.siginfo_address)>(
          siginfo);
  g_exception_information.context_address =
      FromPointerCast<decltype(g_exception_information.context_address)>(
          context);
  g_exception_information.thread_id = gettid();

  if (g_register_at_crash &&
      !CompleteRegistrationByProxy(g_handler_socket, PtraceMode::kSetPtracer)) {
    return;
  }

  if (RequestCrashDump(g_handler_socket)) {
    WaitForCrashDumpDone(g_handler_socket);
  }
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
#define PUSH_ARG(name, value)                                     \
  do {                                                            \
    if (!value.empty()) {                                         \
      argv_strings->push_back(FormatArgumentString(name, value)); \
    }                                                             \
  } while (0);

  PUSH_ARG("database", database.value());
  PUSH_ARG("metrics-dir", metrics_dir.value());
  PUSH_ARG("url", url);
  for (const auto& kv : annotations) {
    PUSH_ARG("annotation", kv.first + '=' + kv.second);
  }
}

void ConvertArgvStrings(const std::vector<std::string> argv_strings,
                        std::vector<const char*> argv) {
  argv->clear();
  argv->reserve(argv_strings.size() + 1);
  for (const auto& arg : argv_strings) {
    argv->push_back(arg.c_str());
  }
  argv->push_back(nullptr);
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
  std::vector<std::string> argv_strings;
  BuildHandlerArgvStrings(handler,
                          database,
                          metrics_dir,
                          url,
                          annotations,
                          arguments,
                          &argv_strings);
  int socks[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, socks) != 0) {
    PLOG(ERROR) << "socketpair";
    return false;
  }
  ScopedFileHandle client_sock(socks[0]);
  ScopedFilehandle server_sock(socks[1]);

  if (fcntl(client_sock.get(), F_SETFD, FD_CLOEXEC) != 0) {
    PLOG(ERROR) << "fcntl";
    return false;
  }

  ClientData data = {} data.registration.client_process_id = getpid();
  data.registration.exception_information_address =
      FromPointerCast<LinuxVMAddress>(&g_exception_information);
  data.registration.use_broker = false;
  data.fd = server_sock.get();

  argv_strings->push_back(
      FormatArgumentString("initial_client_registration", data.ToString()));
  std::vector<const char*> argv;
  ConvertArgvStrings(argv_strings, &argv);

  pid_t pid = fork();
  if (pid < 0) {
    return false;
  }
  if (pid == 0) {
    execv(g_handler_argv[0], &g_handler_argv[0]);
    exit(1);
  }

  g_handler_socket = client_sock.release();
  Signals::InstallCrashHandlers(SignalHandlerForCrash, 0, nullptr);
  return true;
}

bool StartHandlerOnCrash(const base::FilePath& handler,
                         const base::FilePath& database,
                         const base::FilePath& metrics_dir,
                         const std::string& url,
                         const std::map<std::string, std::string>& annotations,
                         const std::vector<std::string>& arguments) {
  BuildHandlerArgvStrings(handler,
                          database,
                          metrics_dir,
                          url,
                          annotations,
                          arguments,
                          g_handler_argv_strings);

  g_handler_argv_strings->push_back(FormatArgumentAddress(
      "trace-parent-with-exception", &g_exception_information));

  ConvertHandlerArgv(*g_handler_argv_strings, g_handler_argv);

  Signals::InstallCrashHandlers(LaunchHandlerForCrash, 0, nullptr);
  return true;
}

bool RegisterByProxy(int socket, PtraceMode mode, bool defer_registration) {
  g_handler_argv_strings->clear();

  g_handler_socket = socket;
  g_register_at_crash = defer_registration;

  if (!defer_registration && !CompleteRegistrationByProxy(socket, mode)) {
    return false;
  }

  switch (mode) {
    case PtraceMode::kSetPtracer:
      Signals::InstallCrashHandlers(SignalHandlerForCrash, 0, nullptr);
      break;
    case PtraceMode::kBroker:
      Signals::InstallCrashHandlers(ForkBrokerForCrash, 0, nullptr);
      break;
    default:
      LOG(ERROR) << "invalid mode";
      return false;
  }
  return true;
}

}  // namespace crashpad
