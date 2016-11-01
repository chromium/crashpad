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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <memory>

#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"
#include "util/misc/implicit_cast.h"
#include "util/numeric/safe_assignment.h"

#if defined(OS_MACOSX)
#include <sys/sysctl.h>
#endif

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

using ScopedDIR = std::unique_ptr<DIR, ScopedDIRCloser>;

// This function implements CloseMultipleNowOrOnExec() using an operating
// system-specific FD directory to determine which file descriptors are open.
// This is an advantage over looping over all possible file descriptors, because
// no attempt needs to be made to close file descriptors that are not open.
bool CloseMultipleNowOrOnExecUsingFDDir(int fd, int preserve_fd) {
#if defined(OS_MACOSX)
  const char kFDDir[] = "/dev/fd";
#elif defined(OS_LINUX) || defined(OS_ANDROID)
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

  dirent* result;
#if defined(OS_LINUX)
  // readdir_r() is deprecated as of glibc 2.24. See
  // https://sourceware.org/bugzilla/show_bug.cgi?id=19056 and
  // https://git.kernel.org/cgit/docs/man-pages/man-pages.git/commit?id=0c52f6d623636a61eacd0f7b7a3bb942793a2a05.
  const char kReaddirName[] = "readdir";
  while ((errno = 0, result = readdir(dir)) != nullptr)
#else
  const char kReaddirName[] = "readdir_r";
  dirent entry;
  while ((errno = readdir_r(dir, &entry, &result)) == 0 && result != nullptr)
#endif
  {
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

  if (errno != 0) {
    PLOG(WARNING) << kReaddirName;
    return false;
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
  // prior to the limit being lowered. On both macOS and Linux glibc, both
  // sysconf() and getdtablesize() return the current RLIMIT_NOFILE value, not
  // the maximum possible file descriptor. For macOS, see 10.11.5
  // Libc-1082.50.1/gen/FreeBSD/sysconf.c sysconf() and 10.11.6
  // xnu-3248.60.10/bsd/kern/kern_descrip.c getdtablesize(). For Linux glibc,
  // see glibc-2.24/sysdeps/posix/sysconf.c __sysconf() and
  // glibc-2.24/sysdeps/posix/getdtsz.c __getdtablesize(). For Android, see
  // 7.0.0 bionic/libc/bionic/sysconf.cpp sysconf() and
  // bionic/libc/bionic/ndk_cruft.cpp getdtablesize().
  int max_fd = implicit_cast<int>(sysconf(_SC_OPEN_MAX));

#if !defined(OS_ANDROID)
  // getdtablesize() was removed effective Android 5.0.0 (API 21). Since it
  // returns the same thing as the sysconf() above, just skip it. See
  // https://android.googlesource.com/platform/bionic/+/462abab12b074c62c0999859e65d5a32ebb41951.
  max_fd = std::max(max_fd, getdtablesize());
#endif

#if !defined(OS_LINUX) || defined(OPEN_MAX)
  // Linux does not provide OPEN_MAX. See
  // https://git.kernel.org/cgit/linux/kernel/git/stable/linux-stable.git/commit/include/linux/limits.h?id=77293034696e3e0b6c8b8fc1f96be091104b3d2b.
  max_fd = std::max(max_fd, OPEN_MAX);
#endif

  // Consult a sysctl to determine the system-wide limit on the maximum number
  // of open files per process. Note that it is possible to change this limit
  // while the system is running, but it’s still a better upper bound than the
  // current RLIMIT_NOFILE value.

#if defined(OS_MACOSX)
  // See 10.11.6 xnu-3248.60.10/bsd/kern/kern_resource.c maxfilesperproc,
  // referenced by dosetrlimit().
  int oid[] = {CTL_KERN, KERN_MAXFILESPERPROC};
  int maxfilesperproc;
  size_t maxfilesperproc_size = sizeof(maxfilesperproc);
  if (sysctl(oid,
             arraysize(oid),
             &maxfilesperproc,
             &maxfilesperproc_size,
             nullptr,
             0) == 0) {
    max_fd = std::max(max_fd, maxfilesperproc);
  } else {
    PLOG(WARNING) << "sysctl";
  }
#elif defined(OS_LINUX) || defined(OS_ANDROID)
  // See linux-4.4.27/fs/file.c sysctl_nr_open, referenced by kernel/sys.c
  // do_prlimit() and kernel/sysctl.c fs_table. Inability to open this file is
  // not considered an error, because /proc may not be available or usable.
  {
    base::ScopedFILE nr_open_file(fopen("/proc/sys/fs/nr_open", "r"));
    if (nr_open_file.get() != nullptr) {
      int nr_open;
      if (fscanf(nr_open_file.get(), "%d\n", &nr_open) == 1 &&
          feof(nr_open_file.get())) {
        max_fd = std::max(max_fd, nr_open);
      } else {
        LOG(WARNING) << "/proc/sys/fs/nr_open format error";
      }
    }
  }
#endif

  for (int entry_fd = fd; entry_fd < max_fd; ++entry_fd) {
    if (entry_fd != preserve_fd) {
      CloseNowOrOnExec(entry_fd, true);
    }
  }
}

}  // namespace crashpad
