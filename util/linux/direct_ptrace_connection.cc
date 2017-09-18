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

#include "util/linux/direct_ptrace_connection.h"

#include <memory>

namespace crashpad {

DirectPtraceConnection::DirectPtraceConnection()
    : PtraceConnection(),
      attachments_(),
      ptracer_() {}

DirectPtraceConnection::~DirectPtraceConnection() {}

bool DirectPtraceConnection::Initialize(pid_t pid) {
  return Attach(pid) && ptracer_.Initialize(pid);
}

bool DirectPtraceConnection::Attach(pid_t tid) {
  std::unique_ptr<ScopedPtraceAttach> attach(new ScopedPtraceAttach);
  if (!attach->ResetAttach(tid)) {
    return false;
  }
  attachments_.push_back(attach.release());
  return true;
}

bool DirectPtraceConnection::Is64Bit(pid_t tid) {
  return true;  // TODO fixme
}

bool DirectPtraceConnection::GetThreadInfo(pid_t tid, ThreadInfo* info) {
  return ptracer_.GetThreadInfo(tid, info);
}

}  // namespace crashpad
