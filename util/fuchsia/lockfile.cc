// Copyright 2018 The Crashpad Authors. All rights reserved.
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

#include "util/fuchsia/lockfile.h"

#include <zircon/device/sysinfo.h>
#include <zircon/process.h>
#include <zircon/syscalls.h>
#include <zircon/types.h>

#include <algorithm>
#include <string>

#include "base/logging.h"
#include "util/file/file_io.h"
#include "util/file/filesystem.h"

namespace crashpad {

// Outline of the approach:
//
// A file is created associated with each path to be locked that's existence
// indicates the lockedness.
//
// Lockfiles are stored in /tmp so that things are automatically cleaned up on
// system restart.
//
// The lockfile name is chosen to be the hash of the given path so that it's
// unique without having to require that the user pass in a separate unique
// name.
//
// When the lockfile is written, the koid of the process is written to the file.
// This allows waiters to recognize a stale lock (that is, when the process
// holding the lock has crashed).
// 
// Stale locks complicate the implementation, unfortunately. The acquisition
// looks like:
//
// - Attempt to create the lockfile with O_EXCL
//   - if the file create succeeds, the lock has been acquired
//     - write this process's koid to the file
//   - if the file create fails:
//     - read the contents of the lockfile
//       - retry the read while there's fewer that sizeof(zx_koid_t) bytes read
//         to handle the locker not having completed the write
//     - determine if the koid named in the file is still valid. If it isn't,
//       unlink the lockfile, and restart the whole process.
//     - otherwise, wait a bit and retry.
//
// A slightly more advanced solution would use a directory watch to be notified
// when a lockfile was removed, however, it would still need to poll with a
// timeout, otherwise if the holder of the lock crashed while a process was
// waiting, it would never wake up. So, stick with a simple poll, since we
// expect contention to be extremely low in real usage. It is also hoped that
// this code can be deleted at some point and replaced with a system service, so
// don't overdo it.
//
// One other note is that the implementation here has to be careful not to use
// something else in util/file that in turn uses locks.

namespace {

base::FilePath LockfileName(const base::FilePath& path) {
  // TODO(scottmg): Maybe SHA256 the realpath and use that.
  std::string result = path.value();
  std::replace(result.begin(), result.end(), '/', '_');
  return base::FilePath("/tmp/" + result + ".__lock__");
}

zx_koid_t GetSelfKoid() {
  zx_info_handle_basic_t info;
  if (zx_object_get_info(zx_process_self(),
                         ZX_INFO_HANDLE_BASIC,
                         &info,
                         sizeof(info),
                         nullptr,
                         nullptr) != ZX_OK) {
    return ZX_KOID_INVALID;
  }
  return info.koid;
}

zx_koid_t ReadKoidFromLockfile(const base::FilePath& lockfile_name) {
  int attempts = 0;
  for (;;) {
    zx_koid_t koid;
    std::string contents;
    if (LoggingReadEntireFile(lockfile_name, &contents) &&
        contents.size() == sizeof(koid)) {
      koid = *reinterpret_cast<zx_koid_t*>(&contents[0]);
      return koid;
    }

    zx_nanosleep(zx_deadline_after(ZX_MSEC(10)));

    ++attempts;
    constexpr int kMaxAttempts = 100;
    if (attempts > kMaxAttempts) {
      return ZX_KOID_INVALID;
    }
  }
}

zx_handle_t GetRootJob() {
  ScopedFileHandle sysinfo(
      LoggingOpenFileForRead(base::FilePath("/dev/misc/sysinfo")));
  if (!sysinfo.is_valid()) {
    return ZX_HANDLE_INVALID;
  }

  zx_handle_t root_job;
  size_t n = ioctl_sysinfo_get_root_job(sysinfo.get(), &root_job);
  if (n != sizeof(root_job)) {
    LOG(ERROR) << "cannot obtain root job";
    return ZX_HANDLE_INVALID;
  }

  return root_job;
}

bool ProcessExists(zx_koid_t koid) {
  zx_handle_t handle = GetRootJob();
  LOG(ERROR) << "root=" << handle;
  return true;
}

}  // namespace

bool BlockingLockFileFuchsia(const base::FilePath& path) {
  for (;;) {
    base::FilePath lockfile_name(LockfileName(path));
    ScopedFileHandle lockfile_excl(
        OpenFileForWrite(lockfile_name,
                         FileWriteMode::kCreateOrFail,
                         FilePermissions::kWorldReadable));
    if (lockfile_excl.is_valid()) {
      zx_koid_t koid = GetSelfKoid();
      if (koid == ZX_KOID_INVALID ||
          !LoggingWriteFile(lockfile_excl.get(), &koid, sizeof(koid))) {
        // If it's not possible to retrieve the koid, or write the koid to the
        // file, things are going very poorly, fail the lock.
        BlockingUnlockFile(path);
        return false;
      }
      return true;
    } else {
      zx_koid_t koid = ReadKoidFromLockfile(lockfile_name);
      if (koid == ZX_KOID_INVALID) {
        // Couldn't read from file after trying for a while. State is
        // indeterminate, release lock and retry.
        if (!BlockingUnlockFile(path)) {
          // If removing the file fails, no choice but to fail out.
          return false;
        }
        continue;
      }
      if (!ProcessExists(koid)) {
        // The lock is invalid, remove the file and retry.
        if (!BlockingUnlockFile(path)) {
          // If removing the file fails, no choice but to fail out.
          return false;
        }
        continue;
      }

      // Otherwise the lock is valid. Wait a little, and then retry.
      zx_nanosleep(zx_deadline_after(ZX_MSEC(100)));
      continue;
    }
  }
}

bool BlockingUnlockFileFuchsia(const base::FilePath& path) {
  return LoggingRemoveFile(LockfileName(path));
}

}  // namespace crashpad
