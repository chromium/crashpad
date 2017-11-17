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

#include "test/mac/exception_swallower.h"

#include <fcntl.h>
#include <sys/socket.h>

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/mac/scoped_mach_port.h"
#include "base/strings/stringprintf.h"
#include "gtest/gtest.h"
#include "test/test_paths.h"
#include "util/file/file_io.h"
#include "util/mach/exception_ports.h"
#include "util/mach/mach_extensions.h"
#include "util/posix/double_fork_and_exec.h"

namespace crashpad {
namespace test {

// static
void ExceptionSwallower::Parent_PrepareForCrashingChild() {
  Get()->SetParent();
}

// static
void ExceptionSwallower::Parent_PrepareForGtestDeathTest() {
  if (testing::FLAGS_gtest_death_test_style == "fast") {
    Parent_PrepareForCrashingChild();
  } else {
    // This is the only other death test style that’s known to gtest.
    DCHECK_EQ(testing::FLAGS_gtest_death_test_style, "threadsafe");
  }
}

// static
void ExceptionSwallower::Child_SwallowExceptions() {
  Get()->SwallowExceptions();
}

ExceptionSwallower::ExceptionSwallower()
    : service_name_(), fd_(), parent_pid_(0) {
  base::FilePath exception_swallower_server_path =
      TestPaths::Executable().DirName().Append("crashpad_exception_swallower");

  // Use socketpair() as a full-duplex pipe().
  int socket_fds[2];
  PCHECK(socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, socket_fds) == 0)
      << "socketpair";

  fd_.reset(socket_fds[0]);
  base::ScopedFD exception_swallower_fd(socket_fds[1]);

  // fd_ is long-lived. Make sure that nobody accidentaly inherits it.
  PCHECK(fcntl(fd_.get(), F_SETFD, FD_CLOEXEC) != -1) << "fcntl";

  // SIGPIPE is undesirable when writing to this socket. Allow broken-pipe
  // writes to fail with EPIPE instead.
  for (size_t index = 0; index < arraysize(socket_fds); ++index) {
    constexpr int value = 1;
    PCHECK(setsockopt(socket_fds[index],
                      SOL_SOCKET,
                      SO_NOSIGPIPE,
                      &value,
                      sizeof(value)) == 0)
        << "setsockopt";
  }

  std::vector<std::string> argv;
  argv.reserve(2);
  argv.push_back(exception_swallower_server_path.value());
  argv.push_back(
      base::StringPrintf("--socket-fd=%d", exception_swallower_fd.get()));

  CHECK(DoubleForkAndExec(argv, exception_swallower_fd.get(), false, nullptr));

  // Close the exception swallower server’s side of the socket, so that it’s the
  // only process that can use it.
  exception_swallower_fd.reset();

  // When the exception swallower server provides its registered service name,
  // it’s ready to go.
  uint8_t service_name_size;
  CheckedReadFileExactly(
      fd_.get(), &service_name_size, sizeof(service_name_size));
  service_name_.resize(service_name_size);
  if (!service_name_.empty()) {
    CheckedReadFileExactly(fd_.get(), &service_name_[0], service_name_.size());
  }

  // Verify that everything’s set up.
  base::mac::ScopedMachSendRight exception_swallower_port(
      BootstrapLookUp(service_name_));
  CHECK(exception_swallower_port.is_valid());
}

ExceptionSwallower::~ExceptionSwallower() {}

// static
ExceptionSwallower* ExceptionSwallower::Get() {
  static ExceptionSwallower* const instance = new ExceptionSwallower();
  return instance;
}

void ExceptionSwallower::SetParent() {
  parent_pid_ = getpid();
}

void ExceptionSwallower::SwallowExceptions() {
  CHECK_NE(getpid(), parent_pid_);

  base::mac::ScopedMachSendRight exception_swallower_port(
      BootstrapLookUp(service_name_));
  CHECK(exception_swallower_port.is_valid());

  ExceptionPorts task_exception_ports(ExceptionPorts::kTargetTypeTask,
                                      TASK_NULL);

  // The mask is similar to the one used by CrashpadClient::UseHandler(), but
  // EXC_CORPSE_NOTIFY is added. This is done for the benefit of tests that
  // crash intentionally with their own custom exception port set for EXC_CRASH.
  // In that case, depending on the actions taken by the EXC_CRASH handler, the
  // exception may be transformed by the kernel into an EXC_CORPSE_NOTIFY, which
  // would be sent to an EXC_CORPSE_NOTIFY handler, normally the system’s crash
  // reporter at the task or host level. See 10.13.0
  // xnu-4570.1.46/bsd/kern/kern_exit.c proc_prepareexit(). Swallowing
  // EXC_CORPSE_NOTIFY at the task level prevents such exceptions from reaching
  // the system’s crash reporter.
  CHECK(task_exception_ports.SetExceptionPort(
      (EXC_MASK_CRASH |
       EXC_MASK_RESOURCE |
       EXC_MASK_GUARD |
       EXC_MASK_CORPSE_NOTIFY) & ExcMaskValid(),
      exception_swallower_port.get(),
      EXCEPTION_DEFAULT,
      THREAD_STATE_NONE));
}

}  // namespace test
}  // namespace crashpad
