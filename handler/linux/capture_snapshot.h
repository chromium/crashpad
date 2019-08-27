// Copyright 2019 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_HANDLER_LINUX_CAPTURE_SNAPSHOT_H_
#define CRASHPAD_HANDLER_LINUX_CAPTURE_SNAPSHOT_H_

#include <sys/types.h>

#include <map>
#include <memory>
#include <string>

#include "snapshot/linux/process_snapshot_linux.h"
#include "snapshot/sanitized/process_snapshot_sanitized.h"
#include "util/linux/exception_handler_protocol.h"
#include "util/linux/ptrace_connection.h"
#include "util/misc/address_types.h"

namespace crashpad {

bool CaptureSnapshot(
    PtraceConnection* connection,
    const ExceptionHandlerProtocol::ClientInformation& info,
    const std::map<std::string, std::string>& process_annotations,
    uid_t client_uid,
    VMAddress requesting_thread_stack_address,
    pid_t* requesting_thread_id,
    std::unique_ptr<ProcessSnapshotLinux>* process_snapshot,
    std::unique_ptr<ProcessSnapshotSanitized>* sanitized_snapshot);

}  // namespace crashpad

#endif  // CRASHPAD_HANDLER_LINUX_CAPTURE_SNAPSHOT_H_
