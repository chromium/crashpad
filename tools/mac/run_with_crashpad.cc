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

#include <errno.h>
#include <getopt.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <map>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "client/crashpad_client.h"
#include "tools/tool_support.h"
#include "util/stdlib/map_insert.h"
#include "util/string/split_string.h"

namespace crashpad {
namespace {

void Usage(const std::string& me) {
  fprintf(stderr,
"Usage: %s [OPTION]... COMMAND [ARG]...\n"
"Start a Crashpad handler and have it handle crashes from COMMAND.\n"
"\n"
"  -h, --handler=HANDLER       invoke HANDLER instead of crashpad_handler\n"
"      --annotation=KEY=VALUE  passed to the handler as an --annotation argument\n"
"      --database=PATH         passed to the handler as its --database argument\n"
"      --url=URL               passed to the handler as its --url argument\n"
"  -a, --argument=ARGUMENT     invoke the handler with ARGUMENT\n"
"      --help                  display this help and exit\n"
"      --version               output version information and exit\n",
          me.c_str());
  ToolSupport::UsageTail(me);
}

int RunWithCrashpadMain(int argc, char* argv[]) {
  const std::string me(basename(argv[0]));

  enum ExitCode {
    kExitSuccess = EXIT_SUCCESS,

    // To differentiate this tool’s errors from errors in the programs it execs,
    // use a high exit code for ordinary failures instead of EXIT_FAILURE. This
    // is the same rationale for using the distinct exit codes for exec
    // failures.
    kExitFailure = 125,

    // Like env, use exit code 126 if the program was found but could not be
    // invoked, and 127 if it could not be found.
    // http://pubs.opengroup.org/onlinepubs/9699919799/utilities/env.html
    kExitExecFailure = 126,
    kExitExecENOENT = 127,
  };

  enum OptionFlags {
    // “Short” (single-character) options.
    kOptionHandler = 'h',
    kOptionArgument = 'a',

    // Long options without short equivalents.
    kOptionLastChar = 255,
    kOptionAnnotation,
    kOptionDatabase,
    kOptionURL,

    // Standard options.
    kOptionHelp = -2,
    kOptionVersion = -3,
  };

  static constexpr option long_options[] = {
      {"handler", required_argument, nullptr, kOptionHandler},
      {"annotation", required_argument, nullptr, kOptionAnnotation},
      {"database", required_argument, nullptr, kOptionDatabase},
      {"url", required_argument, nullptr, kOptionURL},
      {"argument", required_argument, nullptr, kOptionArgument},
      {"help", no_argument, nullptr, kOptionHelp},
      {"version", no_argument, nullptr, kOptionVersion},
      {nullptr, 0, nullptr, 0},
  };

  struct {
    std::string handler;
    std::map<std::string, std::string> annotations;
    std::string database;
    std::string url;
    std::vector<std::string> arguments;
  } options = {};
  options.handler = "crashpad_handler";

  int opt;
  while ((opt = getopt_long(argc, argv, "+a:h:", long_options, nullptr)) !=
         -1) {
    switch (opt) {
      case kOptionHandler: {
        options.handler = optarg;
        break;
      }
      case kOptionAnnotation: {
        std::string key;
        std::string value;
        if (!SplitStringFirst(optarg, '=', &key, &value)) {
          ToolSupport::UsageHint(me, "--annotation requires KEY=VALUE");
          return EXIT_FAILURE;
        }
        std::string old_value;
        if (!MapInsertOrReplace(&options.annotations, key, value, &old_value)) {
          LOG(WARNING) << "duplicate key " << key << ", discarding value "
                       << old_value;
        }
        break;
      }
      case kOptionDatabase: {
        options.database = optarg;
        break;
      }
      case kOptionURL: {
        options.url = optarg;
        break;
      }
      case kOptionArgument: {
        options.arguments.push_back(optarg);
        break;
      }
      case kOptionHelp: {
        Usage(me);
        return kExitSuccess;
      }
      case kOptionVersion: {
        ToolSupport::Version(me);
        return kExitSuccess;
      }
      default: {
        ToolSupport::UsageHint(me, nullptr);
        return kExitFailure;
      }
    }
  }
  argc -= optind;
  argv += optind;

  if (!argc) {
    ToolSupport::UsageHint(me, "COMMAND is required");
    return kExitFailure;
  }

  // Start the handler process and direct exceptions to it.
  CrashpadClient crashpad_client;
  if (!crashpad_client.StartHandler(base::FilePath(options.handler),
                                    base::FilePath(options.database),
                                    base::FilePath(),
                                    options.url,
                                    options.annotations,
                                    options.arguments,
                                    false,
                                    false)) {
    return kExitFailure;
  }

  // Using the remaining arguments, start a new program with the new exception
  // port in effect.
  execvp(argv[0], argv);
  PLOG(ERROR) << "execvp " << argv[0];
  return errno == ENOENT ? kExitExecENOENT : kExitExecFailure;
}

}  // namespace
}  // namespace crashpad

int main(int argc, char* argv[]) {
  return crashpad::RunWithCrashpadMain(argc, argv);
}
