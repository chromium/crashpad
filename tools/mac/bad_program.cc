// Copyright 2020 The Crashpad Authors. All rights reserved.
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

#include <mach/mach.h>
#include <stdio.h>
#include <stdlib.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/mac/scoped_mach_port.h"
#include "util/mach/bootstrap.h"
#include "util/mach/exception_ports.h"
#include "util/mach/mach_extensions.h"
#include "util/misc/paths.h"
#include "util/misc/random_string.h"

namespace crashpad {
namespace {

int BadProgramMain(int argc, char* argv[]) {
  ExceptionPorts task_exception_ports(ExceptionPorts::kTargetTypeTask,
                                      TASK_NULL);
  ExceptionPorts::ExceptionHandlerVector old_handlers;
  CHECK(task_exception_ports.GetExceptionPorts(ExcMaskAll(), &old_handlers));
  CHECK_EQ(old_handlers.size(), 1u);

  const ExceptionPorts::ExceptionHandler old_handler = old_handlers[0];

  // Make sure that this is running under the debugger. That simplifies things
  // because there’s no need to figure out which of multiple handlers is the
  // debugger (or to have to worry about forwarding exceptions intended for
  // another handler to the debugger), and no need to register a long-lived
  // service with the bootstrap server, leaking the mapping. The registration
  // for the debugger’s port should disappear when the debugging session does.
  CHECK_EQ(old_handler.mask, ExcMaskAll()) << "run under lldb";
  CHECK_EQ(old_handler.behavior,
           static_cast<exception_behavior_t>(MACH_EXCEPTION_CODES |
                                             EXCEPTION_DEFAULT))
      << "run under lldb";
  CHECK_EQ(old_handler.flavor,
           static_cast<thread_state_flavor_t>(THREAD_STATE_NONE))
      << "run under lldb";

  // Make sure there aren’t any thread-level handlers messing with things.
  ExceptionPorts thread_exception_ports(ExceptionPorts::kTargetTypeThread,
                                        THREAD_NULL);
  ExceptionPorts::ExceptionHandlerVector thread_handlers;
  CHECK(thread_exception_ports.GetExceptionPorts(ExcMaskValid(),
                                                 &thread_handlers));
  CHECK(thread_handlers.empty());

  std::string base_service_name =
      "org.chromium.crashpad.bad_program." + RandomString();
  std::string forward_to_service_name = base_service_name + ".forward";
  LOG(INFO) << "forward_to_service_name " << forward_to_service_name;
  CHECK(BootstrapRegister(forward_to_service_name.c_str(), old_handler.port));

  static constexpr char kCatchExceptionToolName[] = "catch_exception_tool";
  base::FilePath executable_path;
  CHECK(Paths::Executable(&executable_path));
  base::FilePath catch_exception_tool_path =
      executable_path.DirName().Append(kCatchExceptionToolName);
  std::string catch_exception_service_name = base_service_name + ".catch";

  // Don’t posix_spawn() it because it’d inherit this task’s exception ports.
  printf("Run:\n%s --mach-service %s --forward-to %s --persistent\n",
         catch_exception_tool_path.value().c_str(),
         catch_exception_service_name.c_str(),
         forward_to_service_name.c_str());

  char* line = nullptr;
  size_t line_buf_size;
  getline(&line, &line_buf_size, stdin);
  free(line);

  base::mac::ScopedMachSendRight catch_exception_port =
      BootstrapLookUp(catch_exception_service_name);
  CHECK(catch_exception_port.is_valid());

  CHECK(task_exception_ports.SetExceptionPort(ExcMaskAll(),
                                              catch_exception_port.get(),
                                              old_handler.behavior,
                                              old_handler.flavor));

  // Do something bad now.
  __builtin_trap();

  return EXIT_SUCCESS;
}

}  // namespace
}  // namespace crashpad

int main(int argc, char* argv[]) {
  return crashpad::BadProgramMain(argc, argv);
}
