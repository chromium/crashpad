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

#include "client/crashpad_client.h"

#include <mach/mach.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/stringprintf.h"
#include "util/mach/child_port_handshake.h"
#include "util/mach/exception_ports.h"
#include "util/mach/mach_extensions.h"
#include "util/posix/close_multiple.h"

namespace crashpad {

namespace {

std::string FormatArgumentString(const std::string& name,
                                 const std::string& value) {
  return base::StringPrintf("--%s=%s", name.c_str(), value.c_str());
}

std::string FormatArgumentInt(const std::string& name, int value) {
  return base::StringPrintf("--%s=%d", name.c_str(), value);
}

// Set the exception handler for EXC_CRASH, EXC_RESOURCE, and EXC_GUARD.
//
// EXC_CRASH is how most crashes are received. Most other exception types such
// as EXC_BAD_ACCESS are delivered to a host-level exception handler in the
// kernel where they are converted to POSIX signals. See 10.9.5
// xnu-2422.115.4/bsd/uxkern/ux_exception.c catch_mach_exception_raise(). If a
// core-generating signal (triggered through this hardware mechanism or a
// software mechanism such as abort() sending SIGABRT) is unhandled and the
// process exits, or if the process is killed with SIGKILL for code-signing
// reasons, an EXC_CRASH exception will be sent. See 10.9.5
// xnu-2422.115.4/bsd/kern/kern_exit.c proc_prepareexit().
//
// EXC_RESOURCE and EXC_GUARD do not become signals or EXC_CRASH exceptions. The
// host-level exception handler in the kernel does not receive these exception
// types, and even if it did, it would not map them to signals. Instead, the
// first Mach service loaded by the root (process ID 1) launchd with a boolean
// “ExceptionServer” property in its job dictionary (regardless of its value) or
// with any subdictionary property will become the host-level exception handler
// for EXC_CRASH, EXC_RESOURCE, and EXC_GUARD. See 10.9.5
// launchd-842.92.1/src/core.c job_setup_exception_port(). Normally, this job is
// com.apple.ReportCrash.Root, the systemwide Apple Crash Reporter. Since it is
// impossible to receive EXC_RESOURCE and EXC_GUARD exceptions through the
// EXC_CRASH mechanism, an exception handler must be registered for them by name
// if it is to receive these exception types. The default task-level handler for
// these exception types is set by launchd in a similar manner.
//
// EXC_MASK_RESOURCE and EXC_MASK_GUARD are not available on all systems, and
// the kernel will reject attempts to use them if it does not understand them,
// so AND them with ExcMaskValid(). EXC_MASK_CRASH is always supported.
bool SetCrashExceptionPorts(exception_handler_t exception_handler) {
  ExceptionPorts exception_ports(ExceptionPorts::kTargetTypeTask, TASK_NULL);
  return exception_ports.SetExceptionPort(
      (EXC_MASK_CRASH | EXC_MASK_RESOURCE | EXC_MASK_GUARD) & ExcMaskValid(),
      exception_handler,
      EXCEPTION_STATE_IDENTITY | MACH_EXCEPTION_CODES,
      MACHINE_THREAD_STATE);
}

}  // namespace

CrashpadClient::CrashpadClient()
    : exception_port_() {
}

CrashpadClient::~CrashpadClient() {
}

bool CrashpadClient::StartHandler(
    const base::FilePath& handler,
    const base::FilePath& database,
    const std::string& url,
    const std::map<std::string, std::string>& annotations,
    const std::vector<std::string>& arguments) {
  DCHECK(!exception_port_.is_valid());

  // Set up the arguments for execve() first. These aren’t needed until execve()
  // is called, but it’s dangerous to do this in a child process after fork().
  ChildPortHandshake child_port_handshake;
  base::ScopedFD client_read_fd = child_port_handshake.ClientReadFD();

  // Use handler as argv[0], followed by arguments directed by this method’s
  // parameters and a --handshake-fd argument. |arguments| are added first so
  // that if it erroneously contains an argument such as --url, the actual |url|
  // argument passed to this method will supersede it. In normal command-line
  // processing, the last parameter wins in the case of a conflict.
  std::vector<std::string> argv(1, handler.value());
  argv.reserve(1 + arguments.size() + 2 + annotations.size() + 1);
  for (const std::string& argument : arguments) {
    argv.push_back(argument);
  }
  if (!database.value().empty()) {
    argv.push_back(FormatArgumentString("database", database.value()));
  }
  if (!url.empty()) {
    argv.push_back(FormatArgumentString("url", url));
  }
  for (const auto& kv : annotations) {
    argv.push_back(
        FormatArgumentString("annotation", kv.first + '=' + kv.second));
  }
  argv.push_back(FormatArgumentInt("handshake-fd", client_read_fd.get()));

  // argv_c contains const char* pointers and is terminated by nullptr. argv
  // is required because the pointers in argv_c need to point somewhere, and
  // they can’t point to temporaries such as those returned by
  // FormatArgumentString().
  std::vector<const char*> argv_c;
  argv_c.reserve(argv.size() + 1);
  for (const std::string& argument : argv) {
    argv_c.push_back(argument.c_str());
  }
  argv_c.push_back(nullptr);

  // Double-fork(). The three processes involved are parent, child, and
  // grandchild. The grandchild will become the handler process. The child exits
  // immediately after spawning the grandchild, so the grandchild becomes an
  // orphan and its parent process ID becomes 1. This relieves the parent and
  // child of the responsibility for reaping the grandchild with waitpid() or
  // similar. The handler process is expected to outlive the parent process, so
  // the parent shouldn’t be concerned with reaping it. This approach means that
  // accidental early termination of the handler process will not result in a
  // zombie process.
  pid_t pid = fork();
  if (pid < 0) {
    PLOG(ERROR) << "fork";
    return false;
  }

  if (pid == 0) {
    // Child process.

    // Call setsid(), creating a new process group and a new session, both led
    // by this process. The new process group has no controlling terminal. This
    // disconnects it from signals generated by the parent process’ terminal.
    //
    // setsid() is done in the child instead of the grandchild so that the
    // grandchild will not be a session leader. If it were a session leader, an
    // accidental open() of a terminal device without O_NOCTTY would make that
    // terminal the controlling terminal.
    //
    // It’s not desirable for the handler to have a controlling terminal. The
    // handler monitors clients on its own and manages its own lifetime, exiting
    // when it loses all clients and when it deems it appropraite to do so. It
    // may serve clients in different process groups or sessions than its
    // original client, and receiving signals intended for its original client’s
    // process group could be harmful in that case.
    PCHECK(setsid() != -1) << "setsid";

    pid = fork();
    if (pid < 0) {
      PLOG(FATAL) << "fork";
    }

    if (pid > 0) {
      // Child process.

      // _exit() instead of exit(), because fork() was called.
      _exit(EXIT_SUCCESS);
    }

    // Grandchild process.

    CloseMultipleNowOrOnExec(STDERR_FILENO + 1, client_read_fd.get());

    // &argv_c[0] is a pointer to a pointer to const char data, but because of
    // how C (not C++) works, execvp() wants a pointer to a const pointer to
    // char data. It modifies neither the data nor the pointers, so the
    // const_cast is safe.
    execvp(handler.value().c_str(), const_cast<char* const*>(&argv_c[0]));
    PLOG(FATAL) << "execvp " << handler.value();
  }

  // Parent process.

  client_read_fd.reset();

  // waitpid() for the child, so that it does not become a zombie process. The
  // child normally exits quickly.
  int status;
  pid_t wait_pid = HANDLE_EINTR(waitpid(pid, &status, 0));
  PCHECK(wait_pid != -1) << "waitpid";
  DCHECK_EQ(wait_pid, pid);

  if (WIFSIGNALED(status)) {
    LOG(WARNING) << "intermediate process: signal " << WTERMSIG(status);
  } else if (!WIFEXITED(status)) {
    DLOG(WARNING) << "intermediate process: unknown termination " << status;
  } else if (WEXITSTATUS(status) != EXIT_SUCCESS) {
    LOG(WARNING) << "intermediate process: exit status " << WEXITSTATUS(status);
  }

  // Rendezvous with the handler running in the grandchild process.
  exception_port_.reset(child_port_handshake.RunServer(
      ChildPortHandshake::PortRightType::kSendRight));

  return exception_port_.is_valid();
}

bool CrashpadClient::UseHandler() {
  DCHECK(exception_port_.is_valid());

  return SetCrashExceptionPorts(exception_port_.get());
}

// static
void CrashpadClient::UseSystemDefaultHandler() {
  base::mac::ScopedMachSendRight
      system_crash_reporter_handler(SystemCrashReporterHandler());

  // Proceed even if SystemCrashReporterHandler() failed, setting MACH_PORT_NULL
  // to clear the current exception ports.
  if (!SetCrashExceptionPorts(system_crash_reporter_handler.get())) {
    SetCrashExceptionPorts(MACH_PORT_NULL);
  }
}

}  // namespace crashpad
