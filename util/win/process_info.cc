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

#include "util/win/process_info.h"

#include "base/logging.h"

namespace crashpad {

namespace {

NTSTATUS NtQueryInformationProcess(HANDLE process_handle,
                                   PROCESSINFOCLASS process_information_class,
                                   PVOID process_information,
                                   ULONG process_information_length,
                                   PULONG return_length) {
  static decltype(::NtQueryInformationProcess)* nt_query_information_process =
      reinterpret_cast<decltype(::NtQueryInformationProcess)*>(GetProcAddress(
          LoadLibrary(L"ntdll.dll"), "NtQueryInformationProcess"));
  DCHECK(nt_query_information_process);
  return nt_query_information_process(process_handle,
                                      process_information_class,
                                      process_information,
                                      process_information_length,
                                      return_length);
}

bool IsProcessWow64(HANDLE process_handle) {
  static decltype(IsWow64Process)* is_wow64_process =
      reinterpret_cast<decltype(IsWow64Process)*>(
          GetProcAddress(LoadLibrary(L"kernel32.dll"), "IsWow64Process"));
  if (!is_wow64_process)
    return false;
  BOOL is_wow64;
  if (!is_wow64_process(process_handle, &is_wow64)) {
    PLOG(ERROR) << "IsWow64Process";
    return false;
  }
  return is_wow64;
}

bool ReadUnicodeString(HANDLE process,
                       const UNICODE_STRING& us,
                       std::wstring* result) {
  if (us.Length == 0) {
    result->clear();
    return true;
  }
  DCHECK_EQ(us.Length % sizeof(wchar_t), 0u);
  result->resize(us.Length / sizeof(wchar_t));
  SIZE_T bytes_read;
  if (!ReadProcessMemory(
          process, us.Buffer, &result->operator[](0), us.Length, &bytes_read)) {
    PLOG(ERROR) << "ReadProcessMemory UNICODE_STRING";
    return false;
  }
  if (bytes_read != us.Length) {
    LOG(ERROR) << "ReadProcessMemory UNICODE_STRING incorrect size";
    return false;
  }
  return true;
}

template <class T> bool ReadStruct(HANDLE process, uintptr_t at, T* into) {
  SIZE_T bytes_read;
  if (!ReadProcessMemory(process,
                         reinterpret_cast<const void*>(at),
                         into,
                         sizeof(T),
                         &bytes_read)) {
    // We don't have a name for the type we're reading, so include the signature
    // to get the type of T.
    PLOG(ERROR) << "ReadProcessMemory " << __FUNCSIG__;
    return false;
  }
  if (bytes_read != sizeof(T)) {
    LOG(ERROR) << "ReadProcessMemory " << __FUNCSIG__ << " incorrect size";
    return false;
  }
  return true;
}

// PEB_LDR_DATA in winternl.h doesn't document the trailing
// InInitializationOrderModuleList field. See `dt ntdll!PEB_LDR_DATA`.
struct FULL_PEB_LDR_DATA : public PEB_LDR_DATA {
  LIST_ENTRY InInitializationOrderModuleList;
};

// LDR_DATA_TABLE_ENTRY doesn't include InInitializationOrderLinks, define a
// complete version here. See `dt ntdll!_LDR_DATA_TABLE_ENTRY`.
struct FULL_LDR_DATA_TABLE_ENTRY {
  LIST_ENTRY InLoadOrderLinks;
  LIST_ENTRY InMemoryOrderLinks;
  LIST_ENTRY InInitializationOrderLinks;
  PVOID DllBase;
  PVOID EntryPoint;
  ULONG SizeOfImage;
  UNICODE_STRING FullDllName;
  UNICODE_STRING BaseDllName;
  ULONG Flags;
  WORD LoadCount;
  WORD TlsIndex;
  LIST_ENTRY HashLinks;
  ULONG TimeDateStamp;
  _ACTIVATION_CONTEXT* EntryPointActivationContext;
};

}  // namespace

ProcessInfo::ProcessInfo()
    : process_basic_information_(),
      command_line_(),
      modules_(),
      is_64_bit_(false),
      is_wow64_(false),
      initialized_() {
}

ProcessInfo::~ProcessInfo() {
}

bool ProcessInfo::Initialize(HANDLE process) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  is_wow64_ = IsProcessWow64(process);

  if (is_wow64_) {
    // If it's WoW64, then it's 32-on-64.
    is_64_bit_ = false;
  } else {
    // Otherwise, it's either 32 on 32, or 64 on 64. Use GetSystemInfo() to
    // distinguish between these two cases.
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    is_64_bit_ =
        system_info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64;
  }

#if ARCH_CPU_64_BITS
  if (!is_64_bit_) {
    LOG(ERROR) << "Reading different bitness not yet supported";
    return false;
  }
#else
  if (is_64_bit_) {
    LOG(ERROR) << "Reading x64 process from x86 process not supported";
    return false;
  }
#endif

  ULONG bytes_returned;
  NTSTATUS status =
      crashpad::NtQueryInformationProcess(process,
                                          ProcessBasicInformation,
                                          &process_basic_information_,
                                          sizeof(process_basic_information_),
                                          &bytes_returned);
  if (status < 0) {
    LOG(ERROR) << "NtQueryInformationProcess: status=" << status;
    return false;
  }
  if (bytes_returned != sizeof(process_basic_information_)) {
    LOG(ERROR) << "NtQueryInformationProcess incorrect size";
    return false;
  }

  // Try to read the process environment block.
  PEB peb;
  if (!ReadStruct(process,
                  reinterpret_cast<uintptr_t>(
                      process_basic_information_.PebBaseAddress),
                  &peb)) {
    return false;
  }

  RTL_USER_PROCESS_PARAMETERS process_parameters;
  if (!ReadStruct(process,
                  reinterpret_cast<uintptr_t>(peb.ProcessParameters),
                  &process_parameters)) {
    return false;
  }

  if (!ReadUnicodeString(
          process, process_parameters.CommandLine, &command_line_)) {
    return false;
  }

  FULL_PEB_LDR_DATA peb_ldr_data;
  if (!ReadStruct(process, reinterpret_cast<uintptr_t>(peb.Ldr), &peb_ldr_data))
    return false;

  // Include the first module in the memory order list to get our own name as
  // it's not included in initialization order below.
  std::wstring self_module;
  FULL_LDR_DATA_TABLE_ENTRY self_ldr_data_table_entry;
  if (!ReadStruct(process,
                  reinterpret_cast<uintptr_t>(
                      reinterpret_cast<const char*>(
                          peb_ldr_data.InMemoryOrderModuleList.Flink) -
                      offsetof(FULL_LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks)),
                  &self_ldr_data_table_entry)) {
    return false;
  }
  if (!ReadUnicodeString(
          process, self_ldr_data_table_entry.FullDllName, &self_module)) {
    return false;
  }
  modules_.push_back(self_module);

  // Walk the PEB LDR structure (doubly-linked list) to get the list of loaded
  // modules. We use this method rather than EnumProcessModules to get the
  // modules in initialization order rather than memory order.
  const LIST_ENTRY* last = peb_ldr_data.InInitializationOrderModuleList.Blink;
  FULL_LDR_DATA_TABLE_ENTRY ldr_data_table_entry;
  for (const LIST_ENTRY* cur =
           peb_ldr_data.InInitializationOrderModuleList.Flink;
       ;
       cur = ldr_data_table_entry.InInitializationOrderLinks.Flink) {
    // |cur| is the pointer to the LIST_ENTRY embedded in the
    // FULL_LDR_DATA_TABLE_ENTRY, in the target process's address space. So we
    // need to read from the target, and also offset back to the beginning of
    // the structure.
    if (!ReadStruct(
            process,
            reinterpret_cast<uintptr_t>(reinterpret_cast<const char*>(cur) -
                                        offsetof(FULL_LDR_DATA_TABLE_ENTRY,
                                                 InInitializationOrderLinks)),
            &ldr_data_table_entry)) {
      break;
    }
    // TODO(scottmg): Capture TimeDateStamp, Checksum, etc. too?
    std::wstring module;
    if (!ReadUnicodeString(process, ldr_data_table_entry.FullDllName, &module))
      break;
    modules_.push_back(module);
    if (cur == last)
      break;
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

bool ProcessInfo::Is64Bit() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return is_64_bit_;
}

bool ProcessInfo::IsWow64() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return is_wow64_;
}

pid_t ProcessInfo::ProcessID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return process_basic_information_.UniqueProcessId;
}

pid_t ProcessInfo::ParentProcessID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return process_basic_information_.InheritedFromUniqueProcessId;
}

bool ProcessInfo::CommandLine(std::wstring* command_line) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *command_line = command_line_;
  return true;
}

bool ProcessInfo::Modules(std::vector<std::wstring>* modules) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *modules = modules_;
  return true;
}

}  // namespace crashpad
