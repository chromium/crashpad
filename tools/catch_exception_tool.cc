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
#include <servers/bootstrap.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/mac/mach_logging.h"
#include "tools/tool_support.h"
#include "util/mach/exc_server_variants.h"
#include "util/mach/exception_behaviors.h"
#include "util/mach/mach_extensions.h"
#include "util/mach/mach_message_server.h"
#include "util/mach/symbolic_constants_mach.h"
#include "util/posix/symbolic_constants_posix.h"
#include "util/stdlib/string_number_conversion.h"

namespace {

using namespace crashpad;

struct Options {
  std::string file_path;
  std::string mach_service;
  FILE* file;
  int timeout_secs;
  MachMessageServer::Nonblocking nonblocking;
  MachMessageServer::Persistent persistent;
};

class ExceptionServer : public UniversalMachExcServer {
 public:
  ExceptionServer(const Options& options,
                  const std::string& me,
                  int* exceptions_handled)
      : UniversalMachExcServer(),
        options_(options),
        me_(me),
        exceptions_handled_(exceptions_handled) {}

  virtual kern_return_t CatchMachException(
      exception_behavior_t behavior,
      exception_handler_t exception_port,
      thread_t thread,
      task_t task,
      exception_type_t exception,
      const mach_exception_data_type_t* code,
      mach_msg_type_number_t code_count,
      thread_state_flavor_t* flavor,
      const natural_t* old_state,
      mach_msg_type_number_t old_state_count,
      thread_state_t new_state,
      mach_msg_type_number_t* new_state_count,
      bool* destroy_complex_request) override {
    *destroy_complex_request = true;
    ++*exceptions_handled_;

    fprintf(options_.file,
            "%s: behavior %s, ",
            me_.c_str(),
            ExceptionBehaviorToString(
                behavior, kUseFullName | kUnknownIsNumeric | kUseOr).c_str());

    kern_return_t kr;
    if (ExceptionBehaviorHasIdentity(behavior)) {
      pid_t pid;
      kr = pid_for_task(task, &pid);
      if (kr != KERN_SUCCESS) {
        MACH_LOG(ERROR, kr) << "pid_for_task";
        return KERN_FAILURE;
      }
      fprintf(options_.file, "pid %d, ", pid);

      thread_identifier_info identifier_info;
      mach_msg_type_number_t count = THREAD_IDENTIFIER_INFO_COUNT;
      kr = thread_info(thread,
                       THREAD_IDENTIFIER_INFO,
                       reinterpret_cast<thread_info_t>(&identifier_info),
                       &count);
      if (kr != KERN_SUCCESS) {
        MACH_LOG(ERROR, kr) << "thread_info";
        return KERN_FAILURE;
      }
      fprintf(options_.file, "thread %lld, ", identifier_info.thread_id);
    }

    fprintf(
        options_.file,
        "exception %s, codes[%d] ",
        ExceptionToString(exception, kUseFullName | kUnknownIsNumeric).c_str(),
        code_count);

    for (size_t index = 0; index < code_count; ++index) {
      fprintf(options_.file,
              "%#llx%s",
              code[index],
              index != code_count - 1 ? ", " : "");
    }

    if (exception == EXC_CRASH) {
      mach_exception_code_t original_code_0;
      int signal;
      exception_type_t original_exception =
          ExcCrashRecoverOriginalException(code[0], &original_code_0, &signal);
      fprintf(options_.file,
              ", original exception %s, original code[0] %lld, signal %s",
              ExceptionToString(original_exception,
                                kUseFullName | kUnknownIsNumeric).c_str(),
              original_code_0,
              SignalToString(signal, kUseFullName | kUnknownIsNumeric).c_str());
    }

    if (ExceptionBehaviorHasState(behavior)) {
      // If this is a state-carrying exception, make new_state something valid.
      memcpy(
          new_state,
          old_state,
          std::min(old_state_count, *new_state_count) * sizeof(old_state[0]));
      *new_state_count = old_state_count;

      std::string flavor_string =
          ThreadStateFlavorToString(*flavor, kUseFullName | kUnknownIsNumeric);
      fprintf(options_.file,
              ", flavor %s, old_state_count %d",
              flavor_string.c_str(),
              old_state_count);
    }

    fprintf(options_.file, "\n");
    fflush(options_.file);

    if (exception != EXC_CRASH && exception != kMachExceptionSimulated) {
      // Find another handler.
      return KERN_FAILURE;
    }

    return ExcServerSuccessfulReturnValue(behavior, false);
  }

 private:
  const Options& options_;
  const std::string& me_;
  int* exceptions_handled_;
};

void Usage(const std::string& me) {
  fprintf(stderr,
"Usage: %s -m SERVICE [OPTION]...\n"
"Catch Mach exceptions and display information about them.\n"
"\n"
"  -f, --file=FILE             append information to FILE instead of stdout\n"
"  -m, --mach_service=SERVICE  register SERVICE with the bootstrap server\n"
"  -n, --nonblocking           don't block waiting for an exception to occur\n"
"  -p, --persistent            continue processing exceptions after the first\n"
"  -t, --timeout=TIMEOUT       run for a maximum of TIMEOUT seconds\n"
"      --help                  display this help and exit\n"
"      --version               output version information and exit\n",
          me.c_str());
  ToolSupport::UsageTail(me);
}

}  // namespace

int main(int argc, char* argv[]) {
  const std::string me(basename(argv[0]));

  enum OptionFlags {
    // “Short” (single-character) options.
    kOptionFile = 'f',
    kOptionMachService = 'm',
    kOptionNonblocking = 'n',
    kOptionPersistent = 'p',
    kOptionTimeout = 't',

    // Long options without short equivalents.
    kOptionLastChar = 255,

    // Standard options.
    kOptionHelp = -2,
    kOptionVersion = -3,
  };

  Options options = {};

  const struct option long_options[] = {
      {"file", required_argument, NULL, kOptionFile},
      {"mach_service", required_argument, NULL, kOptionMachService},
      {"nonblocking", no_argument, NULL, kOptionNonblocking},
      {"persistent", no_argument, NULL, kOptionPersistent},
      {"timeout", required_argument, NULL, kOptionTimeout},
      {"help", no_argument, NULL, kOptionHelp},
      {"version", no_argument, NULL, kOptionVersion},
      {NULL, 0, NULL, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "f:m:npt:", long_options, NULL)) !=
         -1) {
    switch (opt) {
      case kOptionFile:
        options.file_path = optarg;
        break;
      case kOptionMachService:
        options.mach_service = optarg;
        break;
      case kOptionNonblocking:
        options.nonblocking = MachMessageServer::kNonblocking;
        break;
      case kOptionPersistent:
        options.persistent = MachMessageServer::kPersistent;
        break;
      case kOptionTimeout:
        if (!StringToNumber(optarg, &options.timeout_secs) ||
            options.timeout_secs <= 0) {
          ToolSupport::UsageHint(me, "-t requires a positive TIMEOUT");
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
        ToolSupport::UsageHint(me, NULL);
        return EXIT_FAILURE;
    }
  }
  argc -= optind;
  argv += optind;

  if (options.mach_service.empty()) {
    ToolSupport::UsageHint(me, "-m is required");
    return EXIT_FAILURE;
  }

  mach_port_t service_port;
  kern_return_t kr = bootstrap_check_in(
      bootstrap_port, options.mach_service.c_str(), &service_port);
  if (kr != BOOTSTRAP_SUCCESS) {
    BOOTSTRAP_LOG(ERROR, kr) << "bootstrap_check_in " << options.mach_service;
    return EXIT_FAILURE;
  }

  base::ScopedFILE file_owner;
  if (options.file_path.empty()) {
    options.file = stdout;
  } else {
    file_owner.reset(fopen(options.file_path.c_str(), "a"));
    if (!file_owner.get()) {
      PLOG(ERROR) << "fopen " << options.file_path;
      return EXIT_FAILURE;
    }
    options.file = file_owner.get();
  }

  int exceptions_handled = 0;
  ExceptionServer exception_server(options, me, &exceptions_handled);

  mach_msg_timeout_t timeout_ms = options.timeout_secs
                                      ? options.timeout_secs * 1000
                                      : MACH_MSG_TIMEOUT_NONE;

  mach_msg_return_t mr = MachMessageServer::Run(&exception_server,
                                                service_port,
                                                MACH_MSG_OPTION_NONE,
                                                options.persistent,
                                                options.nonblocking,
                                                timeout_ms);
  if (mr == MACH_RCV_TIMED_OUT && options.timeout_secs && options.persistent &&
      exceptions_handled) {
    // This is not an error: when exiting on timeout during persistent
    // processing and at least one exception was handled, it’s considered a
    // success.
  } else if (mr != MACH_MSG_SUCCESS) {
    MACH_LOG(ERROR, mr) << "MachMessageServer::Run";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
