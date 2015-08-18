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

#include "snapshot/win/exception_snapshot_win.h"

#include "snapshot/win/cpu_context_win.h"
#include "snapshot/win/process_reader_win.h"
#include "util/win/nt_internals.h"
#include "util/win/process_structs.h"

namespace crashpad {
namespace internal {

ExceptionSnapshotWin::ExceptionSnapshotWin()
    : ExceptionSnapshot(),
      context_union_(),
      context_(),
      codes_(),
      thread_id_(0),
      exception_address_(0),
      exception_flags_(0),
      exception_code_(0),
      initialized_() {
}

ExceptionSnapshotWin::~ExceptionSnapshotWin() {
}

bool ExceptionSnapshotWin::Initialize(ProcessReaderWin* process_reader,
                                      DWORD thread_id,
                                      WinVMAddress exception_pointers_address) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  bool found_thread = false;
  for (const auto& loop_thread : process_reader->Threads()) {
    if (thread_id == loop_thread.id) {
      found_thread = true;
      break;
    }
  }

  if (!found_thread) {
    LOG(ERROR) << "thread ID " << thread_id << "not found in process";
    return false;
  } else {
    thread_id_ = thread_id;
  }

  EXCEPTION_POINTERS exception_pointers;
  if (!process_reader->ReadMemory(exception_pointers_address,
                                  sizeof(EXCEPTION_POINTERS),
                                  &exception_pointers)) {
    LOG(ERROR) << "EXCEPTION_POINTERS read failed";
    return false;
  }
  if (!exception_pointers.ExceptionRecord) {
    LOG(ERROR) << "null ExceptionRecord";
    return false;
  }

  if (process_reader->Is64Bit()) {
    EXCEPTION_RECORD64 first_record;
    if (!process_reader->ReadMemory(
            reinterpret_cast<WinVMAddress>(exception_pointers.ExceptionRecord),
            sizeof(first_record),
            &first_record)) {
      LOG(ERROR) << "ExceptionRecord";
      return false;
    }
    exception_code_ = first_record.ExceptionCode;
    exception_flags_ = first_record.ExceptionFlags;
    exception_address_ = first_record.ExceptionAddress;
    for (DWORD i = 0; i < first_record.NumberParameters; ++i)
      codes_.push_back(first_record.ExceptionInformation[i]);
    if (first_record.ExceptionRecord) {
      // https://code.google.com/p/crashpad/issues/detail?id=43
      LOG(WARNING) << "dropping chained ExceptionRecord";
    }

    context_.architecture = kCPUArchitectureX86_64;
    context_.x86_64 = &context_union_.x86_64;
    // We assume 64-on-64 here in that we're relying on the CONTEXT definition
    // to be the x64 one.
    CONTEXT context_record;
    if (!process_reader->ReadMemory(
            reinterpret_cast<WinVMAddress>(exception_pointers.ContextRecord),
            sizeof(context_record),
            &context_record)) {
      LOG(ERROR) << "ContextRecord";
      return false;
    }
    InitializeX64Context(context_record, context_.x86_64);
  } else {
    CHECK(false) << "TODO(scottmg) x86";
    return false;
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

const CPUContext* ExceptionSnapshotWin::Context() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return &context_;
}

uint64_t ExceptionSnapshotWin::ThreadID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return thread_id_;
}

uint32_t ExceptionSnapshotWin::Exception() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return exception_code_;
}

uint32_t ExceptionSnapshotWin::ExceptionInfo() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return exception_flags_;
}

uint64_t ExceptionSnapshotWin::ExceptionAddress() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return exception_address_;
}

const std::vector<uint64_t>& ExceptionSnapshotWin::Codes() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return codes_;
}

}  // namespace internal
}  // namespace crashpad
