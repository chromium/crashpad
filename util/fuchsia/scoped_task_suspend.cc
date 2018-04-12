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

#include "util/fuchsia/scoped_task_suspend.h"

#include <zircon/process.h>
#include <zircon/syscalls.h>

#include <vector>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/scoped_zx_handle.h"
#include "base/logging.h"

namespace crashpad {

namespace {

std::vector<base::ScopedZxHandle> GetProcessThreads(zx_handle_t process) {
  std::vector<base::ScopedZxHandle> result;

  std::vector<zx_koid_t> threads(100);
  size_t actual_num_threads, available_num_threads;
  for (;;) {
    zx_status_t status = zx_object_get_info(process,
                                            ZX_INFO_PROCESS_THREADS,
                                            &threads[0],
                                            sizeof(threads[0]) * threads.size(),
                                            &actual_num_threads,
                                            &available_num_threads);
    // If the buffer is too small (even zero), the result is still ZX_OK, not
    // ZX_ERR_BUFFER_TOO_SMALL.
    if (status != ZX_OK) {
      ZX_LOG(ERROR, status) << "zx_object_get_info ZX_INFO_PROCESS_THREADS";
      break;
    }
    if (actual_num_threads == available_num_threads) {
      threads.resize(actual_num_threads);
      break;
    }

    // Resize to the expected number next time with a bit extra to attempt to
    // handle the race between here and the next request.
    threads.resize(available_num_threads + 10);
  }

  for (const zx_koid_t thread_koid : threads) {
    zx_handle_t raw_handle;
    zx_status_t status = zx_object_get_child(
        process, thread_koid, ZX_RIGHT_SAME_RIGHTS, &raw_handle);
    if (status != ZX_OK) {
      ZX_LOG(ERROR, status) << "zx_object_get_child";
      // This is expected to fail if there's a race between getting the koid
      // above and requesting the handle, so continue if there's a failure.
      continue;
    }

    result.emplace_back(base::ScopedZxHandle(raw_handle));
  }

  return result;
}

zx_obj_type_t GetHandleType(zx_handle_t handle) {
  zx_info_handle_basic_t basic;
  zx_status_t status = zx_object_get_info(
      handle, ZX_INFO_HANDLE_BASIC, &basic, sizeof(basic), nullptr, nullptr);
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "zx_object_get_info";
    return ZX_OBJ_TYPE_NONE;
  }
  return basic.type;
}

bool SuspendThread(zx_handle_t thread) {
  zx_status_t status = zx_task_suspend(thread);
  ZX_LOG_IF(ERROR, status != ZX_OK, status) << "zx_task_suspend";
  return status == ZX_OK;
}

bool ResumeThread(zx_handle_t thread) {
  zx_status_t status = zx_task_resume(thread, 0);
  ZX_LOG_IF(ERROR, status != ZX_OK, status) << "zx_task_resume";
  return status == ZX_OK;
}

}  // namespace

ScopedTaskSuspend::ScopedTaskSuspend(zx_handle_t task) : task_(task) {
  DCHECK_NE(task_, zx_process_self());
  DCHECK_NE(task_, zx_thread_self());

  zx_obj_type_t type = GetHandleType(task_);
  if (type == ZX_OBJ_TYPE_THREAD) {
    if (!SuspendThread(task_)) {
      task_ = ZX_HANDLE_INVALID;
    }
  } else if (type == ZX_OBJ_TYPE_PROCESS) {
    for (const auto& thread : GetProcessThreads(task_)) {
      SuspendThread(thread.get());
    }
  } else {
    LOG(ERROR) << "unexpected handle type";
    task_ = ZX_HANDLE_INVALID;
  }

}

ScopedTaskSuspend::~ScopedTaskSuspend() {
  if (task_ != ZX_HANDLE_INVALID) {
    zx_obj_type_t type = GetHandleType(task_);
    if (type == ZX_OBJ_TYPE_THREAD) {
      ResumeThread(task_);
    } else if (type == ZX_OBJ_TYPE_PROCESS) {
      for (const auto& thread : GetProcessThreads(task_)) {
        ResumeThread(thread.get());
      }
    } else {
      LOG(ERROR) << "unexpected handle type";
    }
  }
}

}  // namespace crashpad
