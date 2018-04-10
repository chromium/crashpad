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

#include "util/fuchsia/koid_utilities.h"

#include <zircon/device/sysinfo.h>

#include <vector>

#include "base/files/file_path.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "util/file/file_io.h"

namespace crashpad {

namespace {

base::ScopedZxHandle GetRootJob() {
  ScopedFileHandle sysinfo(
      LoggingOpenFileForRead(base::FilePath("/dev/misc/sysinfo")));
  if (!sysinfo.is_valid())
    return base::ScopedZxHandle();

  zx_handle_t root_job;
  size_t n = ioctl_sysinfo_get_root_job(sysinfo.get(), &root_job);
  if (n != sizeof(root_job)) {
    LOG(ERROR) << "unexpected root job size";
    return base::ScopedZxHandle();
  }
  return base::ScopedZxHandle(root_job);
}

std::vector<zx_koid_t> GetChildKoids(zx_handle_t parent, uint32_t child_kind) {
  constexpr size_t kNumExtraKoids = 10u;

  size_t actual = 0;
  size_t available = 0;
  std::vector<zx_koid_t> result;

  // This is inherently racy, but we retry with a bit of slop to try to get a
  // complete list.
  for (int pass = 0; pass < 5; pass++) {
    if (actual <= available)
      result.resize(available + kNumExtraKoids);
    zx_status_t status = zx_object_get_info(parent,
                                            child_kind,
                                            result.data(),
                                            result.size() * sizeof(zx_koid_t),
                                            &actual,
                                            &available);
    if (actual == available) {
      break;
    }
    if (status != ZX_OK) {
      ZX_LOG(ERROR, status) << "zx_object_get_info";
      break;
    }
  }

  result.resize(actual);
  return result;
}

// type can be ZX_INFO_JOB_CHILDREN or ZX_INFO_JOB_PROCESSES.
std::vector<base::ScopedZxHandle> GetChildObjects(
    const base::ScopedZxHandle& job,
    zx_object_info_topic_t type) {
  auto koids = GetChildKoids(job.get(), type);

  std::vector<base::ScopedZxHandle> result;
  result.reserve(koids.size());

  for (zx_koid_t koid : koids) {
    zx_handle_t handle;
    if (zx_object_get_child(job.get(), koid, ZX_RIGHT_SAME_RIGHTS, &handle) ==
        ZX_OK)
      result.push_back(base::ScopedZxHandle(handle));
  }
  return result;
}

bool FindProcess(const base::ScopedZxHandle& job,
                 zx_koid_t koid,
                 base::ScopedZxHandle* out) {
  for (auto& proc : GetChildObjects(job, ZX_INFO_JOB_PROCESSES)) {
    if (GetKoidForHandle(proc.get()) == koid) {
      *out = std::move(proc);
      return true;
    }
  }

  for (const auto& child_job : GetChildObjects(job, ZX_INFO_JOB_CHILDREN)) {
    if (FindProcess(child_job, koid, out))
      return true;
  }

  return false;
}

}  // namespace

zx_koid_t GetKoidForHandle(zx_handle_t object) {
  zx_info_handle_basic_t info;
  zx_status_t status = zx_object_get_info(
      object, ZX_INFO_HANDLE_BASIC, &info, sizeof(info), nullptr, nullptr);
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "zx_object_get_info";
    return ZX_HANDLE_INVALID;
  }
  return info.koid;
}

// TODO(scottmg): This implementation uses some debug/temporary/hacky APIs and
// ioctls that are currently the only way to go from pid to handle. This should
// hopefully eventually be replaced by more or less a single
// zx_debug_something() syscall.
base::ScopedZxHandle GetProcessFromKoid(zx_koid_t koid) {
  base::ScopedZxHandle result;
  if (!FindProcess(GetRootJob(), koid, &result)) {
    LOG(ERROR) << "process " << koid << " not found";
  }
  return result;
}

}  // namespace crashpad
