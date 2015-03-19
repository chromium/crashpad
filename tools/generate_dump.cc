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

#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <mach/mach.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>

#include "base/logging.h"
#include "base/mac/scoped_mach_port.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/stringprintf.h"
#include "minidump/minidump_file_writer.h"
#include "snapshot/mac/process_snapshot_mac.h"
#include "tools/tool_support.h"
#include "util/file/file_writer.h"
#include "util/mach/scoped_task_suspend.h"
#include "util/mach/task_for_pid.h"
#include "util/posix/drop_privileges.h"
#include "util/stdlib/string_number_conversion.h"

namespace crashpad {
namespace {

struct Options {
  std::string dump_path;
  pid_t pid;
  bool suspend;
};

void Usage(const std::string& me) {
  fprintf(stderr,
"Usage: %s [OPTION]... PID\n"
"Generate a minidump file containing a snapshot of a running process.\n"
"\n"
"  -r, --no-suspend   don't suspend the target process during dump generation\n"
"  -o, --output=FILE  write the minidump to FILE instead of minidump.PID\n"
"      --help         display this help and exit\n"
"      --version      output version information and exit\n",
          me.c_str());
  ToolSupport::UsageTail(me);
}

int GenerateDumpMain(int argc, char* argv[]) {
  const std::string me(basename(argv[0]));

  enum OptionFlags {
    // “Short” (single-character) options.
    kOptionOutput = 'o',
    kOptionNoSuspend = 'r',

    // Long options without short equivalents.
    kOptionLastChar = 255,

    // Standard options.
    kOptionHelp = -2,
    kOptionVersion = -3,
  };

  Options options = {};
  options.suspend = true;

  const option long_options[] = {
      {"no-suspend", no_argument, nullptr, kOptionNoSuspend},
      {"output", required_argument, nullptr, kOptionOutput},
      {"help", no_argument, nullptr, kOptionHelp},
      {"version", no_argument, nullptr, kOptionVersion},
      {nullptr, 0, nullptr, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "o:r", long_options, nullptr)) != -1) {
    switch (opt) {
      case kOptionOutput:
        options.dump_path = optarg;
        break;
      case kOptionNoSuspend:
        options.suspend = false;
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

  if (argc != 1) {
    ToolSupport::UsageHint(me, "PID is required");
    return EXIT_FAILURE;
  }

  if (!StringToNumber(argv[0], &options.pid) || options.pid <= 0) {
    fprintf(stderr, "%s: invalid PID: %s\n", me.c_str(), argv[0]);
    return EXIT_FAILURE;
  }

  task_t task = TaskForPID(options.pid);
  if (task == TASK_NULL) {
    return EXIT_FAILURE;
  }
  base::mac::ScopedMachSendRight task_owner(task);

  // This tool may have been installed as a setuid binary so that TaskForPID()
  // could succeed. Drop any privileges now that they’re no longer necessary.
  DropPrivileges();

  if (options.pid == getpid()) {
    if (options.suspend) {
      LOG(ERROR) << "cannot suspend myself";
      return EXIT_FAILURE;
    }
    LOG(WARNING) << "operating on myself";
  }

  if (options.dump_path.empty()) {
    options.dump_path = base::StringPrintf("minidump.%d", options.pid);
  }

  {
    scoped_ptr<ScopedTaskSuspend> suspend;
    if (options.suspend) {
      suspend.reset(new ScopedTaskSuspend(task));
    }

    ProcessSnapshotMac process_snapshot;
    if (!process_snapshot.Initialize(task)) {
      return EXIT_FAILURE;
    }

    FileWriter file_writer;
    if (!file_writer.Open(base::FilePath(options.dump_path),
                          FileWriteMode::kTruncateOrCreate,
                          FilePermissions::kWorldReadable)) {
      return EXIT_FAILURE;
    }

    MinidumpFileWriter minidump;
    minidump.InitializeFromSnapshot(&process_snapshot);
    if (!minidump.WriteEverything(&file_writer)) {
      if (unlink(options.dump_path.c_str()) != 0) {
        PLOG(ERROR) << "unlink";
      }
      return EXIT_FAILURE;
    }
  }

  return EXIT_SUCCESS;
}

}  // namespace
}  // namespace crashpad

int main(int argc, char* argv[]) {
  return crashpad::GenerateDumpMain(argc, argv);
}
