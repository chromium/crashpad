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

#include <winternl.h>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "util/numeric/safe_assignment.h"
#include "util/win/ntstatus_logging.h"
#include "util/win/process_structs.h"

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

template <class T>
bool ReadUnicodeString(HANDLE process,
                       const process_types::UNICODE_STRING<T>& us,
                       std::wstring* result) {
  if (us.Length == 0) {
    result->clear();
    return true;
  }
  DCHECK_EQ(us.Length % sizeof(wchar_t), 0u);
  result->resize(us.Length / sizeof(wchar_t));
  SIZE_T bytes_read;
  if (!ReadProcessMemory(process,
                         reinterpret_cast<const void*>(us.Buffer),
                         &result->operator[](0),
                         us.Length,
                         &bytes_read)) {
    PLOG(ERROR) << "ReadProcessMemory UNICODE_STRING";
    return false;
  }
  if (bytes_read != us.Length) {
    LOG(ERROR) << "ReadProcessMemory UNICODE_STRING incorrect size";
    return false;
  }
  return true;
}

template <class T>
bool ReadStruct(HANDLE process, WinVMAddress at, T* into) {
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

}  // namespace

template <class Traits>
bool GetProcessBasicInformation(HANDLE process,
                                bool is_wow64,
                                ProcessInfo* process_info,
                                WinVMAddress* peb_address) {
  ULONG bytes_returned;
  process_types::PROCESS_BASIC_INFORMATION<Traits> process_basic_information;
  NTSTATUS status =
      crashpad::NtQueryInformationProcess(process,
                                          ProcessBasicInformation,
                                          &process_basic_information,
                                          sizeof(process_basic_information),
                                          &bytes_returned);
  if (!NT_SUCCESS(status)) {
    NTSTATUS_LOG(ERROR, status) << "NtQueryInformationProcess";
    return false;
  }
  if (bytes_returned != sizeof(process_basic_information)) {
    LOG(ERROR) << "NtQueryInformationProcess incorrect size";
    return false;
  }

  // See https://msdn.microsoft.com/en-us/library/windows/desktop/aa384203 on
  // 32 bit being the correct size for HANDLEs for proceses, even on Windows
  // x64. API functions (e.g. OpenProcess) take only a DWORD, so there's no
  // sense in maintaining the top bits.
  process_info->process_id_ =
      static_cast<DWORD>(process_basic_information.UniqueProcessId);
  process_info->inherited_from_process_id_ = static_cast<DWORD>(
      process_basic_information.InheritedFromUniqueProcessId);

  // We now want to read the PEB to gather the rest of our information. The
  // PebBaseAddress as returned above is what we want for 64-on-64 and 32-on-32,
  // but for Wow64, we want to read the 32 bit PEB (a Wow64 process has both).
  // The address of this is found by a second call to NtQueryInformationProcess.
  if (!is_wow64) {
    *peb_address = process_basic_information.PebBaseAddress;
  } else {
    ULONG_PTR wow64_peb_address;
    status = crashpad::NtQueryInformationProcess(process,
                                                 ProcessWow64Information,
                                                 &wow64_peb_address,
                                                 sizeof(wow64_peb_address),
                                                 &bytes_returned);
    if (!NT_SUCCESS(status)) {
      NTSTATUS_LOG(ERROR, status), "NtQueryInformationProcess";
      return false;
    }
    if (bytes_returned != sizeof(wow64_peb_address)) {
      LOG(ERROR) << "NtQueryInformationProcess incorrect size";
      return false;
    }
    *peb_address = wow64_peb_address;
  }

  return true;
}

template <class Traits>
bool ReadProcessData(HANDLE process,
                     WinVMAddress peb_address_vmaddr,
                     ProcessInfo* process_info) {
  Traits::Pointer peb_address;
  if (!AssignIfInRange(&peb_address, peb_address_vmaddr)) {
    LOG(ERROR) << base::StringPrintf("peb address 0x%x out of range",
                                     peb_address_vmaddr);
    return false;
  }

  // Try to read the process environment block.
  process_types::PEB<Traits> peb;
  if (!ReadStruct(process, peb_address, &peb))
    return false;

  process_types::RTL_USER_PROCESS_PARAMETERS<Traits> process_parameters;
  if (!ReadStruct(process, peb.ProcessParameters, &process_parameters))
    return false;

  if (!ReadUnicodeString(process,
                         process_parameters.CommandLine,
                         &process_info->command_line_)) {
    return false;
  }

  process_types::PEB_LDR_DATA<Traits> peb_ldr_data;
  if (!ReadStruct(process, peb.Ldr, &peb_ldr_data))
    return false;

  process_types::LDR_DATA_TABLE_ENTRY<Traits> ldr_data_table_entry;

  // Include the first module in the memory order list to get our the main
  // executable's name, as it's not included in initialization order below.
  if (!ReadStruct(process,
                  reinterpret_cast<WinVMAddress>(
                      reinterpret_cast<const char*>(
                          peb_ldr_data.InMemoryOrderModuleList.Flink) -
                      offsetof(process_types::LDR_DATA_TABLE_ENTRY<Traits>,
                               InMemoryOrderLinks)),
                  &ldr_data_table_entry)) {
    return false;
  }
  ProcessInfo::Module module;
  if (!ReadUnicodeString(
          process, ldr_data_table_entry.FullDllName, &module.name)) {
    return false;
  }
  module.dll_base = ldr_data_table_entry.DllBase;
  module.size = ldr_data_table_entry.SizeOfImage;
  module.timestamp = ldr_data_table_entry.TimeDateStamp;
  process_info->modules_.push_back(module);

  // Walk the PEB LDR structure (doubly-linked list) to get the list of loaded
  // modules. We use this method rather than EnumProcessModules to get the
  // modules in initialization order rather than memory order.
  Traits::Pointer last = peb_ldr_data.InInitializationOrderModuleList.Blink;
  for (Traits::Pointer cur = peb_ldr_data.InInitializationOrderModuleList.Flink;
       ;
       cur = ldr_data_table_entry.InInitializationOrderLinks.Flink) {
    // |cur| is the pointer to the LIST_ENTRY embedded in the
    // LDR_DATA_TABLE_ENTRY, in the target process's address space. So we need
    // to read from the target, and also offset back to the beginning of the
    // structure.
    if (!ReadStruct(process,
                    reinterpret_cast<WinVMAddress>(
                        reinterpret_cast<const char*>(cur) -
                        offsetof(process_types::LDR_DATA_TABLE_ENTRY<Traits>,
                                 InInitializationOrderLinks)),
                    &ldr_data_table_entry)) {
      break;
    }
    // TODO(scottmg): Capture Checksum, etc. too?
    if (!ReadUnicodeString(
            process, ldr_data_table_entry.FullDllName, &module.name)) {
      break;
    }
    module.dll_base = ldr_data_table_entry.DllBase;
    module.size = ldr_data_table_entry.SizeOfImage;
    module.timestamp = ldr_data_table_entry.TimeDateStamp;
    process_info->modules_.push_back(module);
    if (cur == last)
      break;
  }

  return true;
}

ProcessInfo::Module::Module() : name(), dll_base(0), size(0), timestamp() {
}

ProcessInfo::Module::~Module() {
}

ProcessInfo::ProcessInfo()
    : process_id_(),
      inherited_from_process_id_(),
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

#if ARCH_CPU_32_BITS
  if (is_64_bit_) {
    LOG(ERROR) << "Reading x64 process from x86 process not supported";
    return false;
  }
#endif

  WinVMAddress peb_address;
#if ARCH_CPU_64_BITS
  bool result = GetProcessBasicInformation<process_types::internal::Traits64>(
      process, is_wow64_, this, &peb_address);
#else
  bool result = GetProcessBasicInformation<process_types::internal::Traits32>(
      process, false, this, &peb_address);
#endif  // ARCH_CPU_64_BITS

  if (!result) {
    LOG(ERROR) << "GetProcessBasicInformation failed";
    return false;
  }

  result = is_64_bit_ ? ReadProcessData<process_types::internal::Traits64>(
                                 process, peb_address, this)
                           : ReadProcessData<process_types::internal::Traits32>(
                                 process, peb_address, this);
  if (!result) {
    LOG(ERROR) << "ReadProcessData failed";
    return false;
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
  return process_id_;
}

pid_t ProcessInfo::ParentProcessID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return inherited_from_process_id_;
}

bool ProcessInfo::CommandLine(std::wstring* command_line) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *command_line = command_line_;
  return true;
}

bool ProcessInfo::Modules(std::vector<Module>* modules) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *modules = modules_;
  return true;
}

}  // namespace crashpad
