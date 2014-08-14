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

#include "util/posix/process_util.h"

#include <string.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include "base/basictypes.h"

namespace crashpad {

bool ProcessArgumentsForPID(pid_t pid, std::vector<std::string>* argv) {
  // The format of KERN_PROCARGS2 is explained in 10.9.2 adv_cmds-153/ps/print.c
  // getproclline(). It is an int (argc) followed by the executable’s string
  // area. The string area consists of NUL-terminated strings, beginning with
  // the executable path, and then starting on an aligned boundary, all of the
  // elements of argv, envp, and applev.

  // It is possible for a process to exec() in between the two sysctl() calls
  // below. If that happens, and the string area of the new program is larger
  // than that of the old one, args_size_estimate will be too small. To detect
  // this situation, the second sysctl() attempts to fetch args_size_estimate +
  // 1 bytes, expecting to only receive args_size_estimate. If it gets the extra
  // byte, it indicates that the string area has grown, and the sysctl() pair
  // will be retried a limited number of times.

  size_t args_size_estimate;
  size_t args_size;
  std::string args;
  int tries = 3;
  do {
    int mib[] = {CTL_KERN, KERN_PROCARGS2, pid};
    int rv = sysctl(mib, arraysize(mib), NULL, &args_size_estimate, NULL, 0);
    if (rv != 0) {
      return false;
    }

    args_size = args_size_estimate + 1;
    args.resize(args_size);
    rv = sysctl(mib, arraysize(mib), &args[0], &args_size, NULL, 0);
    if (rv != 0) {
      return false;
    }
  } while (args_size == args_size_estimate + 1 && tries--);

  if (args_size == args_size_estimate + 1) {
    return false;
  }

  // KERN_PROCARGS2 needs to at least contain argc.
  if (args_size < sizeof(int)) {
    return false;
  }
  args.resize(args_size);

  // Get argc.
  int argc;
  memcpy(&argc, &args[0], sizeof(argc));

  // Find the end of the executable path.
  size_t start_pos = sizeof(argc);
  size_t nul_pos = args.find('\0', start_pos);
  if (nul_pos == std::string::npos) {
    return false;
  }

  // Find the beginning of the string area.
  start_pos = args.find_first_not_of('\0', nul_pos);
  if (start_pos == std::string::npos) {
    return false;
  }

  std::vector<std::string> local_argv;
  while (argc-- && nul_pos != std::string::npos) {
    nul_pos = args.find('\0', start_pos);
    local_argv.push_back(args.substr(start_pos, nul_pos - start_pos));
    start_pos = nul_pos + 1;
  }

  if (argc >= 0) {
    // Not every argument was recovered.
    return false;
  }

  argv->swap(local_argv);
  return true;
}

}  // namespace crashpad
