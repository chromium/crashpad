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
#include "util/win/process_structs.h"
#include "util/win/scoped_handle.h"
#include "util/win/time.h"

namespace crashpad {

namespace {

NTSTATUS NtQuerySystemInformation(
    SYSTEM_INFORMATION_CLASS system_information_class,
    PVOID system_information,
    ULONG system_information_length,
    PULONG return_length) {
  static decltype(::NtQuerySystemInformation)* nt_query_system_information =
      reinterpret_cast<decltype(::NtQuerySystemInformation)*>(GetProcAddress(
          LoadLibrary(L"ntdll.dll"), "NtQuerySystemInformation"));
  DCHECK(nt_query_system_information);
  return nt_query_system_information(system_information_class,
                                     system_information,
                                     system_information_length,
                                     return_length);
}

// The 4th argument is CLIENT_ID*, but as we can't typedef that, we simply cast
// to void* here.
typedef NTSTATUS(WINAPI* NtOpenThreadFunction)(
    PHANDLE ThreadHandle,
    ACCESS_MASK DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    const void* ClientId);

template <class Traits>
NTSTATUS NtOpenThread(PHANDLE thread_handle,
                      ACCESS_MASK desired_access,
                      POBJECT_ATTRIBUTES object_attributes,
                      const process_types::CLIENT_ID<Traits>* client_id) {
  static NtOpenThreadFunction nt_open_thread =
      reinterpret_cast<NtOpenThreadFunction>(
          GetProcAddress(LoadLibrary(L"ntdll.dll"), "NtOpenThread"));
  DCHECK(nt_open_thread);
  return nt_open_thread(thread_handle,
                        desired_access,
                        object_attributes,
                        static_cast<const void*>(client_id));
}

NTSTATUS NtQueryInformationThread(HANDLE thread_handle,
                                  THREADINFOCLASS thread_information_class,
                                  PVOID thread_information,
                                  ULONG thread_information_length,
                                  PULONG return_length) {
  static decltype(::NtQueryInformationThread)* nt_query_information_thread =
      reinterpret_cast<decltype(::NtQueryInformationThread)*>(GetProcAddress(
          LoadLibrary(L"ntdll.dll"), "NtQueryInformationThread"));
  DCHECK(nt_query_information_thread);
  return nt_query_information_thread(thread_handle,
                                     thread_information_class,
                                     thread_information,
                                     thread_information_length,
                                     return_length);
}

// Copied from ntstatus.h because um/winnt.h conflicts with general inclusion of
// ntstatus.h.
#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xC0000023L)
#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004L)

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
uint32_t GetThreadSuspendCount(
    const process_types::SYSTEM_EXTENDED_THREAD_INFORMATION<Traits>&
        thread_info) {
  // Wait reason values are from KWAIT_REASON in wdm.h. We don't need all of
  // them, so just declare the one we need.
  const ULONG kWaitReasonSuspended = 5;

  // Kernel mode enumerations for thread state come from
  // http://www.nirsoft.net/kernel_struct/vista/KTHREAD_STATE.html and
  // https://msdn.microsoft.com/en-us/library/system.diagnostics.threadstate(v=vs.110).aspx
  const ULONG kThreadStateWaiting = 5;
  const ULONG kThreadStateGateWait = 8;

  bool suspended = (thread_info.ThreadState == kThreadStateWaiting ||
                    thread_info.ThreadState == kThreadStateGateWait) &&
                   thread_info.WaitReason == kWaitReasonSuspended;
  if (!suspended)
    return 0;

  HANDLE thread_handle;
  ACCESS_MASK query_access = THREAD_QUERY_LIMITED_INFORMATION;
  OBJECT_ATTRIBUTES object_attributes;
  InitializeObjectAttributes(&object_attributes, nullptr, 0, nullptr, nullptr);
  NTSTATUS status = crashpad::NtOpenThread(
      &thread_handle, query_access, &object_attributes, &thread_info.ClientId);
  if (!NT_SUCCESS(status)) {
    LOG(WARNING) << "couldn't open thread to retrieve suspend count";
    // Fall back to something semi-reasonable. We know we're suspended at this
    // point, so just return 1.
    return 1;
  }

  // Take ownership of this handle so we close on exit. NtClose and CloseHandle
  // are identical.
  ScopedKernelHANDLE handle(thread_handle);

  // From ntddk.h. winternl.h defines THREADINFOCLASS, but only one value.
  const int kThreadSuspendCount = 35;
  ULONG suspend_count;
  status = crashpad::NtQueryInformationThread(
      handle.get(),
      static_cast<THREADINFOCLASS>(kThreadSuspendCount),
      &suspend_count,
      sizeof(suspend_count),
      nullptr);
  if (!NT_SUCCESS(status)) {
    LOG(WARNING) << "NtQueryInformationThread failed" << std::hex << status;
    return 1;
  }

  return suspend_count;
}

}  // namespace

ProcessReaderWin::Thread::Thread()
    : id(0),
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
      initialized_threads_(false),
      initialized_() {
}

ProcessReaderWin::~ProcessReaderWin() {
}

bool ProcessReaderWin::Initialize(HANDLE process) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  process_ = process;
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
    PLOG(ERROR) << "ReadMemory at " << at << " of " << num_bytes << " failed";
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
    thread.suspend_count = GetThreadSuspendCount(thread_info);

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
      thread.stack_region_address = tib.StackBase;
      // Note, "backwards" because of direction of stack growth.
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
