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

#include "handler/fuchsia/exception_handler_server.h"

#include <zircon/syscalls/exception.h>
#include <zircon/syscalls/port.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "handler/fuchsia/crash_report_exception_handler.h"
#include "util/fuchsia/koid_utilities.h"
#include "util/fuchsia/scoped_task_suspend.h"
#include "util/fuchsia/system_exception_port_key.h"

namespace crashpad {

ExceptionHandlerServer::ExceptionHandlerServer(
    base::ScopedZxHandle root_job,
    base::ScopedZxHandle exception_port)
    : root_job_(std::move(root_job)),
      exception_port_(std::move(exception_port)) {}

ExceptionHandlerServer::~ExceptionHandlerServer() = default;

void ExceptionHandlerServer::Run(CrashReportExceptionHandler* handler) {
  while (true) {
    zx_port_packet_t packet;
    zx_status_t status =
        zx_port_wait(exception_port_.get(), ZX_TIME_INFINITE, &packet, 1);
    if (status != ZX_OK) {
      ZX_LOG(ERROR, status) << "zx_port_wait, aborting";
      return;
    }

    if (packet.key != kSystemExceptionPortKey) {
      LOG(ERROR) << "unexpected packet key, ignoring";
      continue;
    }

    // TODO(scottmg): Currently, just use the global helper
    // GetProcessFromKoid(), but at some point that will probably break. At that
    // point, root_job_ will need to be used instead to give the search
    // somewhere to start.
    base::ScopedZxHandle process(GetProcessFromKoid(packet.exception.pid));
    if (!process.is_valid()) {
      LOG(ERROR) << "unable to get process handle for process, ignoring";
      continue;
    }

    ScopedTaskSuspend suspend(process.get());

    zx_handle_t thread_raw;
    status = zx_object_get_child(
        process.get(), packet.exception.tid, ZX_RIGHT_SAME_RIGHTS, &thread_raw);
    if (status != ZX_OK) {
      ZX_LOG(ERROR, status) << "zx_object_get_child thread";
      continue;
    }
    base::ScopedZxHandle thread(thread_raw);

    bool result = handler->HandleException(
        packet.type, process.get(), thread.get(), packet.exception.tid);
    if (!result) {
      LOG(ERROR) << "HandleException failed";
    }

    // Resuming with ZX_RESUME_TRY_NEXT chains to the next handler. In normal
    // operation, there won't be another beyond this one, which will result in
    // the kernel terminating the process.
    status =
        zx_task_resume(thread.get(), ZX_RESUME_EXCEPTION | ZX_RESUME_TRY_NEXT);
    if (status != ZX_OK) {
      ZX_LOG(ERROR, status) << "zx_task_resume";
    }
  }
}

}  // namespace crashpad
