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

namespace crashpad {

const size_t PtraceBroker::Request::kMaxReadMemorySize;

PtraceBroker::PtraceBroker(int sock, bool is_64_bit)
    : ptracer_(is_64_bit), sock_(sock) {}

PtraceBroker::~PtraceBroker() = default;

bool PtraceBroker::Run() {
  while (true) {
    Request request = {};
    if (!ReadFileExactly(sock_, &request, sizeof(request))) {
      return false;
    }

    if (request.version != Request::kVersion) {
      return false;
    }

    switch (request.type) {
      case Request::Type::kAttach: {
        ScopedPtraceAttach attach;
        Bool status =
            attach.ResetAttach(request.tid) ? Bool::kTrue : Bool::kFalse;
        if (!WriteFile(sock_, &status, sizeof(status))) {
          return false;
        }

        // Store the attachment on the stack.
        if (status == Bool::kTrue) {
          return Run();
        }
        continue;
      }

      case Request::Type::kIs64Bit: {
        Bool is_64_bit = ptracer_.Is64Bit() ? Bool::kTrue : Bool::kFalse;
        if (!WriteFile(sock_, &is_64_bit, sizeof(is_64_bit))) {
          return false;
        }
        continue;
      }

      case Request::Type::kGetThreadInfo: {
        GetThreadInfoResponse response;
        response.success = ptracer_.GetThreadInfo(request.tid, &response.info)
                               ? Bool::kTrue
                               : Bool::kFalse;
        if (!WriteFile(sock_, &response, sizeof(response))) {
          return false;
        }
        continue;
      }

      case Request::Type::kReadMemory: {
        Bool success;
        char buffer[Request::kMaxReadMemorySize];
        if (request.iov.size <= Request::kMaxReadMemorySize &&
            ptracer_.ReadMemory(
                request.tid, request.iov.base, request.iov.size, buffer)) {
          success = Bool::kTrue;
          if (!WriteFile(sock_, &success, sizeof(success)) ||
              !WriteFile(sock_, buffer, request.iov.size)) {
            return false;
          }
        } else {
          success = Bool::kFalse;
          if (!WriteFile(sock_, &success, sizeof(success))) {
            return false;
          }
        }
        continue;
      }

      case Request::Type::kExit:
        return true;
    }
    DCHECK(false);
    return false;
  }
}

}  // namespace crashpad
