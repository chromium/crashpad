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

#include "util/linux/exception_handler_client.h"

#include <errno.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"
#include "util/file/file_io.h"
#include "util/linux/ptrace_broker.h"
#include "util/misc/from_pointer_cast.h"
#include "util/posix/signals.h"

#if defined(OS_ANDROID)
#include <android/api-level.h>
#endif

namespace crashpad {

namespace {

class ScopedSigprocmaskRestore {
 public:
  explicit ScopedSigprocmaskRestore(const sigset_t& set_to_block)
      : orig_mask_(), mask_is_set_(false) {
    mask_is_set_ = sigprocmask(SIG_BLOCK, &set_to_block, &orig_mask_) == 0;
    DPLOG_IF(ERROR, !mask_is_set_) << "sigprocmask";
  }

  ~ScopedSigprocmaskRestore() {
    if (mask_is_set_ && sigprocmask(SIG_SETMASK, &orig_mask_, nullptr) != 0) {
      DPLOG(ERROR) << "sigprocmask";
    }
  }

 private:
  sigset_t orig_mask_;
  bool mask_is_set_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSigprocmaskRestore);
};

}  // namespace

ExceptionHandlerClient::ExceptionHandlerClient(int sock, bool multiple_clients)
    : server_sock_(sock),
      ptracer_(-1),
      can_set_ptracer_(true),
      multiple_clients_(multiple_clients) {}

ExceptionHandlerClient::~ExceptionHandlerClient() = default;

int ExceptionHandlerClient::RequestCrashDump(
    const ExceptionHandlerProtocol::ClientInformation& info) {
  VMAddress sp = FromPointerCast<VMAddress>(&sp);

  if (multiple_clients_) {
    return SignalCrashDump(info, sp);
  }

  int status = SendCrashDumpRequest(info, sp);
  if (status != 0) {
    return status;
  }
  return WaitForCrashDumpComplete();
}

int ExceptionHandlerClient::SetPtracer(pid_t pid) {
  if (ptracer_ == pid) {
    return 0;
  }

  if (!can_set_ptracer_) {
    return EPERM;
  }

  if (prctl(PR_SET_PTRACER, pid, 0, 0, 0) == 0) {
    return 0;
  }
  return errno;
}

void ExceptionHandlerClient::SetCanSetPtracer(bool can_set_ptracer) {
  can_set_ptracer_ = can_set_ptracer;
}

int ExceptionHandlerClient::SignalCrashDump(
    const ExceptionHandlerProtocol::ClientInformation& info,
    VMAddress stack_pointer) {
  // TODO(jperaza): Use lss for system calls when sys_sigtimedwait() exists.
  // https://crbug.com/crashpad/265
  sigset_t dump_done_sigset;
  sigemptyset(&dump_done_sigset);
  sigaddset(&dump_done_sigset, ExceptionHandlerProtocol::kDumpDoneSignal);
  ScopedSigprocmaskRestore scoped_block(dump_done_sigset);

  int status = SendCrashDumpRequest(info, stack_pointer);
  if (status != 0) {
    return status;
  }

#if defined(OS_ANDROID) && __ANDROID_API__ < 23
  // sigtimedwait() wrappers aren't available on Android until API 23 but this
  // can use the lss wrapper when it's available.
  NOTREACHED();
#else
  siginfo_t siginfo = {};
  timespec timeout;
  timeout.tv_sec = 5;
  timeout.tv_nsec = 0;
  if (HANDLE_EINTR(sigtimedwait(&dump_done_sigset, &siginfo, &timeout)) < 0) {
    return errno;
  }
#endif

  return 0;
}

int ExceptionHandlerClient::SendCrashDumpRequest(
    const ExceptionHandlerProtocol::ClientInformation& info,
    VMAddress stack_pointer) {
  ExceptionHandlerProtocol::ClientToServerMessage message;
  message.type =
      ExceptionHandlerProtocol::ClientToServerMessage::kCrashDumpRequest;
  message.requesting_thread_stack_address = stack_pointer;
  message.client_info = info;

  iovec iov;
  iov.iov_base = &message;
  iov.iov_len = sizeof(message);

  msghdr msg;
  msg.msg_name = nullptr;
  msg.msg_namelen = 0;
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;

  ucred creds;
  creds.pid = getpid();
  creds.uid = geteuid();
  creds.gid = getegid();

  char cmsg_buf[CMSG_SPACE(sizeof(creds))];
  msg.msg_control = cmsg_buf;
  msg.msg_controllen = sizeof(cmsg_buf);

  cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
  cmsg->cmsg_level = SOL_SOCKET;
  cmsg->cmsg_type = SCM_CREDENTIALS;
  cmsg->cmsg_len = CMSG_LEN(sizeof(creds));
  *reinterpret_cast<ucred*>(CMSG_DATA(cmsg)) = creds;

  if (HANDLE_EINTR(sendmsg(server_sock_, &msg, MSG_NOSIGNAL)) < 0) {
    PLOG(ERROR) << "sendmsg";
    return errno;
  }

  return 0;
}

int ExceptionHandlerClient::WaitForCrashDumpComplete() {
  ExceptionHandlerProtocol::ServerToClientMessage message;

  // If the server hangs up, ReadFileExactly will return false without setting
  // errno.
  errno = 0;
  while (ReadFileExactly(server_sock_, &message, sizeof(message))) {
    switch (message.type) {
      case ExceptionHandlerProtocol::ServerToClientMessage::kTypeForkBroker: {
        Signals::InstallDefaultHandler(SIGCHLD);

        pid_t pid = fork();
        if (pid <= 0) {
          ExceptionHandlerProtocol::Errno error = pid < 0 ? errno : 0;
          if (!WriteFile(server_sock_, &error, sizeof(error))) {
            return errno;
          }
        }

        if (pid < 0) {
          continue;
        }

        if (pid == 0) {
#if defined(ARCH_CPU_64_BITS)
          constexpr bool am_64_bit = true;
#else
          constexpr bool am_64_bit = false;
#endif  // ARCH_CPU_64_BITS

          PtraceBroker broker(server_sock_, getppid(), am_64_bit);
          _exit(broker.Run());
        }

        int status = 0;
        pid_t child = HANDLE_EINTR(waitpid(pid, &status, 0));
        DCHECK_EQ(child, pid);

        if (child == pid && status != 0) {
          return status;
        }
        continue;
      }

      case ExceptionHandlerProtocol::ServerToClientMessage::kTypeSetPtracer: {
        ExceptionHandlerProtocol::Errno result = SetPtracer(message.pid);
        if (!WriteFile(server_sock_, &result, sizeof(result))) {
          return errno;
        }
        continue;
      }

      case ExceptionHandlerProtocol::ServerToClientMessage::
          kTypeCrashDumpComplete:
      case ExceptionHandlerProtocol::ServerToClientMessage::
          kTypeCrashDumpFailed:
        return 0;
    }

    DCHECK(false);
  }

  return errno;
}

}  // namespace crashpad
