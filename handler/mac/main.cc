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

#include <getopt.h>
#include <libgen.h>
#include <stdlib.h>

#include <string>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "client/crash_report_database.h"
#include "tools/tool_support.h"
#include "handler/mac/crash_report_exception_handler.h"
#include "handler/mac/exception_handler_server.h"
#include "util/mach/child_port_handshake.h"
#include "util/posix/close_stdio.h"
#include "util/stdlib/string_number_conversion.h"

namespace crashpad {
namespace {

void Usage(const std::string& me) {
  fprintf(stderr,
"Usage: %s [OPTION]...\n"
"Crashpad's exception handler server.\n"
"\n"
"      --database=PATH    store the crash report database at PATH\n"
"      --handshake-fd=FD  establish communication with the client over FD\n"
"      --help             display this help and exit\n"
"      --version          output version information and exit\n",
          me.c_str());
  ToolSupport::UsageTail(me);
}

int HandlerMain(int argc, char* argv[]) {
  const std::string me(basename(argv[0]));

  enum OptionFlags {
    // Long options without short equivalents.
    kOptionLastChar = 255,
    kOptionDatabase,
    kOptionHandshakeFD,

    // Standard options.
    kOptionHelp = -2,
    kOptionVersion = -3,
  };

  struct {
    const char* database;
    int handshake_fd;
  } options = {};
  options.handshake_fd = -1;

  const struct option long_options[] = {
      {"database", required_argument, nullptr, kOptionDatabase},
      {"handshake-fd", required_argument, nullptr, kOptionHandshakeFD},
      {"help", no_argument, nullptr, kOptionHelp},
      {"version", no_argument, nullptr, kOptionVersion},
      {nullptr, 0, nullptr, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "", long_options, nullptr)) != -1) {
    switch (opt) {
      case kOptionDatabase:
        options.database = optarg;
        break;
      case kOptionHandshakeFD:
        if (!StringToNumber(optarg, &options.handshake_fd) ||
            options.handshake_fd < 0) {
          ToolSupport::UsageHint(me,
                                 "--handshake-fd requires a file descriptor");
          return EXIT_FAILURE;
        }
        break;
      case kOptionHelp:
        Usage(me);
        return EXIT_SUCCESS;
      case kOptionVersion:
        ToolSupport::Version(me);
        return EXIT_SUCCESS;
      default:
        ToolSupport::UsageHint(me, nullptr);
        return EXIT_FAILURE;
    }
  }
  argc -= optind;
  argv += optind;

  if (options.handshake_fd < 0) {
    ToolSupport::UsageHint(me, "--handshake-fd is required");
    return EXIT_FAILURE;
  }

  if (!options.database) {
    ToolSupport::UsageHint(me, "--database is required");
    return EXIT_FAILURE;
  }

  if (argc) {
    ToolSupport::UsageHint(me, nullptr);
    return EXIT_FAILURE;
  }

  CloseStdinAndStdout();

  ExceptionHandlerServer exception_handler_server;

  ChildPortHandshake::RunClient(options.handshake_fd,
                                exception_handler_server.receive_port(),
                                MACH_MSG_TYPE_MAKE_SEND);

  scoped_ptr<CrashReportDatabase> database(
      CrashReportDatabase::Initialize(base::FilePath(options.database)));
  if (!database) {
    return EXIT_FAILURE;
  }

  CrashReportExceptionHandler exception_handler(database.get());

  exception_handler_server.Run(&exception_handler);

  return EXIT_SUCCESS;
}

}  // namespace
}  // namespace crashpad

int main(int argc, char* argv[]) {
  return crashpad::HandlerMain(argc, argv);
}
