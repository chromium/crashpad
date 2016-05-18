// Copyright 2015 The Crashpad Authors. All rights reserved.
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

#include "util/win/scoped_process_suspend.h"

#include <winternl.h>

#include "base/logging.h"

namespace crashpad {

ScopedProcessSuspend::ScopedProcessSuspend(HANDLE process)
    : process_(process), suspended_(false) {
  typedef NTSTATUS(__stdcall * NtSuspendProcessFunc)(HANDLE);
  static NtSuspendProcessFunc func = reinterpret_cast<NtSuspendProcessFunc>(
      GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtSuspendProcess"));
  NTSTATUS status = func(process_);
  if (NT_SUCCESS(status))
    suspended_ = true;
  else
    NTSTATUS_LOG(ERROR, status) << "NtSuspendProcess";
}

ScopedProcessSuspend::~ScopedProcessSuspend() {
  if (suspended_) {
    typedef NTSTATUS(__stdcall * NtResumeProcessFunc)(HANDLE);
    static NtResumeProcessFunc func = reinterpret_cast<NtResumeProcessFunc>(
        GetProcAddress(GetModuleHandle(L"ntdll.dll"), "NtResumeProcess"));
    NTSTATUS status = func(process_);
    if (!NT_SUCCESS(status))
      NTSTATUS_LOG(ERROR, status) << "NtResumeProcess";
  }
}

}  // namespace crashpad
