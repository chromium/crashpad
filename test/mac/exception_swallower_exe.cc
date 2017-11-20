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

#include <getopt.h>
#include <libgen.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "handler/mac/exception_handler_server.h"
#include "tools/tool_support.h"
#include "util/file/file_io.h"
#include "util/mach/exc_server_variants.h"
#include "util/mach/mach_extensions.h"
#include "util/misc/random_string.h"
#include "util/posix/close_stdio.h"
#include "util/stdlib/string_number_conversion.h"
#include "util/thread/thread.h"

namespace crashpad {
namespace {

// A Mach exception handler that accepts all exceptions but doesn’t do anything
// with any of them.
class SwallowingExceptionHandler : public UniversalMachExcServer::Interface {
 public:
  SwallowingExceptionHandler() {}
  ~SwallowingExceptionHandler() {}

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
    *destroy_complex_request = true;

    // Swallow.

    ExcServerCopyState(
        behavior, old_state, old_state_count, new_state, new_state_count);
    return ExcServerSuccessfulReturnValue(exception, behavior, false);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SwallowingExceptionHandler);
};

// Watches a file descriptor, and when it reaches end-of-file, asks the
// ExceptionHandlerServer to stop. Because the file descriptor is one end of a
// socketpair(), and the other end is kept open in the client process,
// end-of-file will be reached when the client process terminates.
class EOFWatcherThread : public Thread {
 public:
  EOFWatcherThread(int fd, ExceptionHandlerServer* exception_handler_server)
      : Thread(),
        exception_handler_server_(exception_handler_server),
        fd_(fd) {}

 private:
  void ThreadMain() override {
    char c;
    ssize_t rv = ReadFile(fd_.get(), &c, 1);
    PCHECK(rv >= 0) << internal::kNativeReadFunctionName;
    CHECK(rv == 0);

    exception_handler_server_->Stop();
  }

  ExceptionHandlerServer* exception_handler_server_;  // weak
  base::ScopedFD fd_;

  DISALLOW_COPY_AND_ASSIGN(EOFWatcherThread);
};

void Usage(const std::string& me) {
  fprintf(stderr,
"Usage: %s [OPTION]...\n"
"Crashpad's exception swallower.\n"
"\n"
"      --socket-fd=FD       synchronize with the client over FD\n"
"      --help               display this help and exit\n"
"      --version            output version information and exit\n",
          me.c_str());
  ToolSupport::UsageTail(me);
}

int ExceptionSwallowerMain(int argc, char* argv[]) {
  const std::string me(basename(argv[0]));

  enum OptionFlags {
    // Long options without short equivalents.
    kOptionLastChar = 255,
    kOptionSocketFD,

    // Standard options.
    kOptionHelp = -2,
    kOptionVersion = -3,
  };

  struct {
    int socket_fd;
  } options = {};
  options.socket_fd = -1;

  const option long_options[] = {
      {"socket-fd", required_argument, nullptr, kOptionSocketFD},
      {"help", no_argument, nullptr, kOptionHelp},
      {"version", no_argument, nullptr, kOptionVersion},
      {nullptr, 0, nullptr, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "", long_options, nullptr)) != -1) {
    switch (opt) {
      case kOptionSocketFD:
        if (!StringToNumber(optarg, &options.socket_fd) ||
            options.socket_fd <= STDERR_FILENO) {
          ToolSupport::UsageHint(me, "--socket-fd requires a file descriptor");
          return EXIT_FAILURE;
        }
        break;
      case kOptionHelp: {
        Usage(me);
        return EXIT_SUCCESS;
      }
      case kOptionVersion: {
        ToolSupport::Version(me);
        return EXIT_SUCCESS;
      }
      default: {
        ToolSupport::UsageHint(me, nullptr);
        return EXIT_FAILURE;
      }
    }
  }
  argc -= optind;
  argv += optind;

  if (options.socket_fd < 0) {
    ToolSupport::UsageHint(me, "--socket-fd is required");
    return EXIT_FAILURE;
  }

  if (argc) {
    ToolSupport::UsageHint(me, nullptr);
    return EXIT_FAILURE;
  }

  CloseStdinAndStdout();

  // Build a service name. Include the PID of the client at the other end of the
  // socket, so that the service name has a meaningful relation back to the
  // client that started this server process. A simple getppid() won’t do
  // because the client started this process with a double-fork().
  pid_t peer_pid;
  socklen_t peer_pid_size = base::checked_cast<socklen_t>(sizeof(peer_pid));
  PCHECK(getsockopt(options.socket_fd,
                    SOL_LOCAL,
                    LOCAL_PEERPID,
                    &peer_pid,
                    &peer_pid_size) == 0)
      << "getsockopt";
  CHECK_EQ(peer_pid_size, sizeof(peer_pid));

  std::string service_name =
      base::StringPrintf("org.chromium.crashpad.test.exception_swallower.%d.%s",
                         peer_pid,
                         RandomString().c_str());

  base::mac::ScopedMachReceiveRight receive_right(
      BootstrapCheckIn(service_name));
  CHECK(receive_right.is_valid());

  // Tell the client that the service has been checked in, providing the
  // service name.
  uint8_t service_name_size = base::checked_cast<uint8_t>(service_name.size());
  CheckedWriteFile(
      options.socket_fd, &service_name_size, sizeof(service_name_size));
  CheckedWriteFile(
      options.socket_fd, service_name.c_str(), service_name.size());

  ExceptionHandlerServer exception_handler_server(std::move(receive_right),
                                                  true);

  EOFWatcherThread eof_watcher_thread(options.socket_fd,
                                      &exception_handler_server);
  eof_watcher_thread.Start();

  // This runs until stopped by eof_watcher_thread.
  SwallowingExceptionHandler swallowing_exception_handler;
  exception_handler_server.Run(&swallowing_exception_handler);

  eof_watcher_thread.Join();

  return EXIT_SUCCESS;
}

}  // namespace
}  // namespace crashpad

int main(int argc, char* argv[]) {
  return crashpad::ExceptionSwallowerMain(argc, argv);
}
