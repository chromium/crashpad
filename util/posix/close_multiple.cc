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

#include "util/posix/close_multiple.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"
#include "util/misc/implicit_cast.h"
#include "util/numeric/safe_assignment.h"

// Everything in this file is expected to execute between fork() and exec(),
// so everything called here must be acceptable in this context. However,
// logging code that is not expected to execute under normal circumstances is
// currently permitted.

namespace crashpad {
namespace {

// This function attempts to close |fd| or mark it as close-on-exec. On systems
// where close-on-exec is attempted, a failure to mark it close-on-exec will be
// followed by an attempt to close it. |ebadf_ok| should be set to |true| if
// the caller is attempting to close the file descriptor “blind,” that is,
// without knowledge that it is or is not a valid file descriptor.
void CloseNowOrOnExec(int fd, bool ebadf_ok) {
  int rv;

#if defined(OS_MACOSX)
  // Try to set close-on-exec, to avoid attempting to close a guarded FD with
  // a close guard set.
  rv = fcntl(fd, F_SETFD, FD_CLOEXEC);
  if (rv != -1 || (ebadf_ok && errno == EBADF)) {
    return;
  }
  PLOG(WARNING) << "fcntl";
#endif

  rv = IGNORE_EINTR(close(fd));
  if (rv != 0 && !(ebadf_ok && errno == EBADF)) {
    PLOG(WARNING) << "close";
  }
}

struct ScopedDIRCloser {
  void operator()(DIR* dir) const {
    if (dir) {
      if (closedir(dir) < 0) {
        PLOG(ERROR) << "closedir";
      }
    }
  }
};

using ScopedDIR = scoped_ptr<DIR, ScopedDIRCloser>;

// This function implements CloseMultipleNowOrOnExec() using an operating
// system-specific FD directory to determine which file descriptors are open.
// This is an advantage over looping over all possible file descriptors, because
// no attempt needs to be made to close file descriptors that are not open.
bool CloseMultipleNowOrOnExecUsingFDDir(int fd, int preserve_fd) {
#if defined(OS_MACOSX)
  const char kFDDir[] = "/dev/fd";
#elif defined(OS_LINUX)
  const char kFDDir[] = "/proc/self/fd";
#endif

  DIR* dir = opendir(kFDDir);
  if (!dir) {
    PLOG(WARNING) << "opendir";
    return false;
  }

  ScopedDIR dir_owner(dir);

  int dir_fd = dirfd(dir);
  if (dir_fd == -1) {
    PLOG(WARNING) << "dirfd";
    return false;
  }

  dirent entry;
  dirent* result;
  int rv;
  while ((rv = readdir_r(dir, &entry, &result)) == 0 && result != nullptr) {
    const char* entry_name = &(*result->d_name);
    if (strcmp(entry_name, ".") == 0 || strcmp(entry_name, "..") == 0) {
      continue;
    }

    char* end;
    long entry_fd_long = strtol(entry_name, &end, 10);
    if (entry_name[0] == '\0' || *end) {
      LOG(ERROR) << "unexpected entry " << entry_name;
      return false;
    }

    int entry_fd;
    if (!AssignIfInRange(&entry_fd, entry_fd_long)) {
      LOG(ERROR) << "out-of-range fd " << entry_name;
      return false;
    }

    if (entry_fd >= fd && entry_fd != preserve_fd && entry_fd != dir_fd) {
      CloseNowOrOnExec(entry_fd, false);
    }
  }

  return true;
}

}  // namespace

void CloseMultipleNowOrOnExec(int fd, int preserve_fd) {
  if (CloseMultipleNowOrOnExecUsingFDDir(fd, preserve_fd)) {
    return;
  }

  // Fallback: close every file descriptor starting at |fd| and ending at the
  // system’s file descriptor limit. Check a few values and use the highest as
  // the limit, because these may be based on the file descriptor limit set by
  // setrlimit(), and higher-numbered file descriptors may have been opened
  // prior to the limit being lowered. For Mac OS X, see 10.9.2
  // Libc-997.90.3/gen/FreeBSD/sysconf.c sysconf() and 10.9.4
  // xnu-2422.110.17/bsd/kern/kern_descrip.c getdtablesize(), which both return
  // the current RLIMIT_NOFILE value, not the maximum possible file descriptor.
  int max_fd = std::max(implicit_cast<int>(sysconf(_SC_OPEN_MAX)), OPEN_MAX);
  max_fd = std::max(max_fd, getdtablesize());

  for (int entry_fd = fd; entry_fd < max_fd; ++entry_fd) {
    if (entry_fd != preserve_fd) {
      CloseNowOrOnExec(entry_fd, true);
    }
  }
}

}  // namespace crashpad
