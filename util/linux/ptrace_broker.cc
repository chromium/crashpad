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

#include "util/linux/ptrace_broker.h"

#include "base/logging.h"
#include "util/file/file_io.h"
#include "util/linux/scoped_ptrace_attach.h"
#include "util/misc/as_underlying_type.h"

namespace crashpad {

PtraceBroker::PtraceBroker(int sock, bool is_64_bit)
    : sock_(sock), ptracer_(is_64_bit) {}

PtraceBroker::~PtraceBroker() = default;

bool PtraceBroker::Run() {
  while (true) {
    Request request;
    if (!LoggingReadFileExactly(sock_, &request, sizeof(request))) {
      return false;
    }

    if (request.type == Request::Type::kAttach) {
      ScopedPtraceAttach attach;
      Bool status =
          attach.ResetAttach(request.tid) ? Bool::kTrue : Bool::kFalse;
      if (!LoggingWriteFile(sock_, &status, sizeof(status))) {
        return false;
      }

      // Store the attachment on the stack.
      if (status == Bool::kTrue) {
        return Run();
      }
      continue;
    }

    if (request.type == Request::Type::kIs64Bit) {
      Bool is_64_bit = ptracer_.Is64Bit() ? Bool::kTrue : Bool::kFalse;
      if (!LoggingWriteFile(sock_, &is_64_bit, sizeof(is_64_bit))) {
        return false;
      }
      continue;
    }

    if (request.type == Request::Type::kGetThreadInfo) {
      GetThreadInfoResponse response;
      response.success = ptracer_.GetThreadInfo(request.tid, &response.info)
                             ? Bool::kTrue
                             : Bool::kFalse;
      if (!LoggingWriteFile(sock_, &response, sizeof(response))) {
        return false;
      }
      continue;
    }

    if (request.type == Request::Type::kExit) {
      return true;
    }

    LOG(ERROR) << "Invalid request " << AsUnderlyingType(request.type);
    return false;
  }
}

}  // namespace crashpad
