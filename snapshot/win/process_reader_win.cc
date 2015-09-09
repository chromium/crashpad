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

#include "snapshot/win/process_reader_win.h"

#include <winternl.h>

#include "base/memory/scoped_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "util/win/nt_internals.h"
#include "util/win/process_structs.h"
#include "util/win/scoped_handle.h"
#include "util/win/time.h"

namespace crashpad {

namespace {

// Gets a pointer to the process information structure after a given one, or
// null when iteration is complete, assuming they've been retrieved in a block
// via NtQuerySystemInformation().
template <class Traits>
process_types::SYSTEM_PROCESS_INFORMATION<Traits>* NextProcess(
    process_types::SYSTEM_PROCESS_INFORMATION<Traits>* process) {
  ULONG offset = process->NextEntryOffset;
  if (offset == 0)
    return nullptr;
  return reinterpret_cast<process_types::SYSTEM_PROCESS_INFORMATION<Traits>*>(
      reinterpret_cast<uint8_t*>(process) + offset);
}

//! \brief Retrieves the SYSTEM_PROCESS_INFORMATION for a given process.
//!
//! The returned pointer points into the memory block stored by \a buffer.
//! Ownership of \a buffer is transferred to the caller.
//!
//! \return Pointer to the process' data, or nullptr if it was not found or on
//!     error. On error, a message will be logged.
template <class Traits>
process_types::SYSTEM_PROCESS_INFORMATION<Traits>* GetProcessInformation(
    HANDLE process_handle,
    scoped_ptr<uint8_t[]>* buffer) {
  ULONG buffer_size = 16384;
  buffer->reset(new uint8_t[buffer_size]);
  NTSTATUS status;
  // This must be in retry loop, as we're racing with process creation on the
  // system to find a buffer large enough to hold all process information.
  for (int tries = 0; tries < 20; ++tries) {
    const int kSystemExtendedProcessInformation = 57;
    status = crashpad::NtQuerySystemInformation(
        static_cast<SYSTEM_INFORMATION_CLASS>(
            kSystemExtendedProcessInformation),
        reinterpret_cast<void*>(buffer->get()),
        buffer_size,
        &buffer_size);
    if (status == STATUS_BUFFER_TOO_SMALL ||
        status == STATUS_INFO_LENGTH_MISMATCH) {
      // Add a little extra to try to avoid an additional loop iteration. We're
      // racing with system-wide process creation between here and the next call
      // to NtQuerySystemInformation().
      buffer_size += 4096;
      buffer->reset(new uint8_t[buffer_size]);
    } else {
      break;
    }
  }

  if (!NT_SUCCESS(status)) {
    LOG(ERROR) << "NtQuerySystemInformation failed: " << std::hex << status;
    return nullptr;
  }

  process_types::SYSTEM_PROCESS_INFORMATION<Traits>* process =
      reinterpret_cast<process_types::SYSTEM_PROCESS_INFORMATION<Traits>*>(
          buffer->get());
  DWORD process_id = GetProcessId(process_handle);
  do {
    if (process->UniqueProcessId == process_id)
      return process;
  } while (process = NextProcess(process));

  LOG(ERROR) << "process " << process_id << " not found";
  return nullptr;
}

template <class Traits>
HANDLE OpenThread(const process_types::SYSTEM_EXTENDED_THREAD_INFORMATION<
    Traits>& thread_info) {
  HANDLE handle;
  ACCESS_MASK query_access = THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME;
  OBJECT_ATTRIBUTES object_attributes;
  InitializeObjectAttributes(&object_attributes, nullptr, 0, nullptr, nullptr);
  NTSTATUS status = crashpad::NtOpenThread(
      &handle, query_access, &object_attributes, &thread_info.ClientId);
  if (!NT_SUCCESS(status)) {
    LOG(ERROR) << "NtOpenThread failed";
    return nullptr;
  }
  return handle;
}

// It's necessary to suspend the thread to grab CONTEXT. SuspendThread has a
// side-effect of returning the SuspendCount of the thread on success, so we
// fill out these two pieces of semi-unrelated data in the same function.
template <class Traits>
void FillThreadContextAndSuspendCount(
    const process_types::SYSTEM_EXTENDED_THREAD_INFORMATION<Traits>&
        thread_info,
    ProcessReaderWin::Thread* thread,
    ProcessSuspensionState suspension_state) {
  // Don't suspend the thread if it's this thread. This is really only for test
  // binaries, as we won't be walking ourselves, in general.
  bool is_current_thread = thread_info.ClientId.UniqueThread ==
                           reinterpret_cast<process_types::TEB<Traits>*>(
                               NtCurrentTeb())->ClientId.UniqueThread;

  ScopedKernelHANDLE thread_handle(OpenThread(thread_info));

  // TODO(scottmg): Handle cross-bitness in this function.

  if (is_current_thread) {
    DCHECK(suspension_state == ProcessSuspensionState::kRunning);
    thread->suspend_count = 0;
    RtlCaptureContext(&thread->context);
  } else {
    DWORD previous_suspend_count = SuspendThread(thread_handle.get());
    if (previous_suspend_count == -1) {
      PLOG(ERROR) << "SuspendThread failed";
      return;
    }
    DCHECK(previous_suspend_count > 0 ||
           suspension_state == ProcessSuspensionState::kRunning);
    thread->suspend_count =
        previous_suspend_count -
        (suspension_state == ProcessSuspensionState::kSuspended ? 1 : 0);

    memset(&thread->context, 0, sizeof(thread->context));
    thread->context.ContextFlags = CONTEXT_ALL;
    if (!GetThreadContext(thread_handle.get(), &thread->context)) {
      PLOG(ERROR) << "GetThreadContext failed";
      return;
    }

    if (!ResumeThread(thread_handle.get())) {
      PLOG(ERROR) << "ResumeThread failed";
    }
  }
}

}  // namespace

ProcessReaderWin::Thread::Thread()
    : context(),
      id(0),
      teb(0),
      stack_region_address(0),
      stack_region_size(0),
      suspend_count(0),
      priority_class(0),
      priority(0) {
}

ProcessReaderWin::ProcessReaderWin()
    : process_(INVALID_HANDLE_VALUE),
      process_info_(),
      threads_(),
      modules_(),
      suspension_state_(),
      initialized_threads_(false),
      initialized_() {
}

ProcessReaderWin::~ProcessReaderWin() {
}

bool ProcessReaderWin::Initialize(HANDLE process,
                                  ProcessSuspensionState suspension_state) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  process_ = process;
  suspension_state_ = suspension_state;
  process_info_.Initialize(process);

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

bool ProcessReaderWin::ReadMemory(WinVMAddress at,
                                  WinVMSize num_bytes,
                                  void* into) {
  SIZE_T bytes_read;
  if (!ReadProcessMemory(process_,
                         reinterpret_cast<void*>(at),
                         into,
                         base::checked_cast<SIZE_T>(num_bytes),
                         &bytes_read) ||
      num_bytes != bytes_read) {
    PLOG(ERROR) << "ReadMemory at 0x" << std::hex << at << std::dec << " of "
                << num_bytes << " bytes failed";
    return false;
  }
  return true;
}

bool ProcessReaderWin::StartTime(timeval* start_time) const {
  FILETIME creation, exit, kernel, user;
  if (!GetProcessTimes(process_, &creation, &exit, &kernel, &user)) {
    PLOG(ERROR) << "GetProcessTimes";
    return false;
  }
  *start_time = FiletimeToTimevalEpoch(creation);
  return true;
}

bool ProcessReaderWin::CPUTimes(timeval* user_time,
                                timeval* system_time) const {
  FILETIME creation, exit, kernel, user;
  if (!GetProcessTimes(process_, &creation, &exit, &kernel, &user)) {
    PLOG(ERROR) << "GetProcessTimes";
    return false;
  }
  *user_time = FiletimeToTimevalInterval(user);
  *system_time = FiletimeToTimevalInterval(kernel);
  return true;
}

const std::vector<ProcessReaderWin::Thread>& ProcessReaderWin::Threads() {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  if (initialized_threads_)
    return threads_;

  initialized_threads_ = true;

  DCHECK(threads_.empty());

#if ARCH_CPU_32_BITS
  using SizeTraits = process_types::internal::Traits32;
#else
  using SizeTraits = process_types::internal::Traits64;
#endif
  scoped_ptr<uint8_t[]> buffer;
  process_types::SYSTEM_PROCESS_INFORMATION<SizeTraits>* process_information =
      GetProcessInformation<SizeTraits>(process_, &buffer);
  if (!process_information)
    return threads_;

  for (unsigned long i = 0; i < process_information->NumberOfThreads; ++i) {
    const process_types::SYSTEM_EXTENDED_THREAD_INFORMATION<SizeTraits>&
        thread_info = process_information->Threads[i];
    Thread thread;
    thread.id = thread_info.ClientId.UniqueThread;

    FillThreadContextAndSuspendCount(thread_info, &thread, suspension_state_);

    // TODO(scottmg): I believe we could reverse engineer the PriorityClass from
    // the Priority, BasePriority, and
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ms685100 .
    // MinidumpThreadWriter doesn't handle it yet in any case, so investigate
    // both of those at the same time if it's useful.
    thread.priority_class = NORMAL_PRIORITY_CLASS;

    thread.priority = thread_info.Priority;
    thread.teb = thread_info.TebBase;

    // While there are semi-documented fields in the thread structure called
    // StackBase and StackLimit, they don't appear to be correct in practice (or
    // at least, I don't know how to interpret them). Instead, read the TIB
    // (Thread Information Block) which is the first element of the TEB, and use
    // its stack fields.
    process_types::NT_TIB<SizeTraits> tib;
    if (ReadMemory(thread_info.TebBase, sizeof(tib), &tib)) {
      // Note, "backwards" because of direction of stack growth.
      thread.stack_region_address = tib.StackLimit;
      thread.stack_region_size = tib.StackBase - tib.StackLimit;
    }
    threads_.push_back(thread);
  }

  return threads_;
}

const std::vector<ProcessInfo::Module>& ProcessReaderWin::Modules() {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  if (!process_info_.Modules(&modules_)) {
    LOG(ERROR) << "couldn't retrieve modules";
  }

  return modules_;
}

}  // namespace crashpad
