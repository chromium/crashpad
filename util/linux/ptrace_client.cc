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

#include "util/linux/ptrace_client.h"

#include "util/file/file_io.h"
#include "util/linux/ptrace_broker.h"

namespace crashpad {

namespace {

bool AttachImpl(int sock, pid_t tid) {
  PtraceBroker::Request request;
  request.type = PtraceBroker::Request::Type::kAttach;
  request.tid = tid;
  if (!LoggingWriteFile(sock, &request, sizeof(request))) {
    return false;
  }

  PtraceBroker::Bool success;
  if (!LoggingReadFileExactly(sock, &success, sizeof(success))) {
    return false;
  }
  return success == PtraceBroker::Bool::kTrue;
}

}  // namespace

PtraceClient::PtraceClient()
    : PtraceConnection(),
      sock_(kInvalidFileHandle),
      pid_(-1),
      is_64_bit_(false),
      initialized_() {}

PtraceClient::~PtraceClient() {
  if (sock_ != kInvalidFileHandle) {
    PtraceBroker::Request request;
    request.type = PtraceBroker::Request::Type::kExit;
    LoggingWriteFile(sock_, &request, sizeof(request));
  }
}

bool PtraceClient::Initialize(int sock, pid_t pid) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);
  sock_ = sock;
  pid_ = pid;

  if (!AttachImpl(sock_, pid_)) {
    return false;
  }

  PtraceBroker::Request request;
  request.type = PtraceBroker::Request::Type::kIs64Bit;
  request.tid = pid_;

  if (!LoggingWriteFile(sock_, &request, sizeof(request))) {
    return false;
  }

  PtraceBroker::Bool is_64_bit;
  if (!LoggingReadFileExactly(sock_, &is_64_bit, sizeof(is_64_bit))) {
    return false;
  }
  is_64_bit_ = is_64_bit == PtraceBroker::Bool::kTrue;

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

pid_t PtraceClient::GetProcessID() {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return pid_;
}

bool PtraceClient::Attach(pid_t tid) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return AttachImpl(sock_, tid);
}

bool PtraceClient::Is64Bit() {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return is_64_bit_;
}

bool PtraceClient::GetThreadInfo(pid_t tid, ThreadInfo* info) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  PtraceBroker::Request request;
  request.type = PtraceBroker::Request::Type::kGetThreadInfo;
  request.tid = tid;
  if (!LoggingWriteFile(sock_, &request, sizeof(request))) {
    return false;
  }

  PtraceBroker::GetThreadInfoResponse response;
  if (!LoggingReadFileExactly(sock_, &response, sizeof(response))) {
    return false;
  }
  if (response.success == PtraceBroker::Bool::kTrue) {
    *info = response.info;
    return true;
  }
  return false;
}

bool PtraceClient::Read(VMAddress address, size_t size, void* buffer) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  char* buffer_c = reinterpret_cast<char*>(buffer);

  while (size > 0) {
    size_t to_read = std::min(size, PtraceBroker::Request::kMaxReadMemorySize);

    PtraceBroker::Request request;
    request.type = PtraceBroker::Request::Type::kReadMemory;
    request.tid = pid_;
    request.iov.base = address;
    request.iov.size = to_read;
    if (!LoggingWriteFile(sock_, &request, sizeof(request))) {
      return false;
    }

    PtraceBroker::Bool success;
    if (!LoggingReadFileExactly(sock_, &success, sizeof(success)) ||
        success != PtraceBroker::Bool::kTrue) {
      return false;
    }

    if (!LoggingReadFileExactly(sock_, buffer_c, to_read)) {
      return false;
    }

    size -= to_read;
    address += to_read;
    buffer_c += to_read;
  }
  return true;
}

}  // namespace crashpad
