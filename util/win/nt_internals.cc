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

#include "util/win/nt_internals.h"

#include "base/logging.h"

namespace crashpad {

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

NTSTATUS NtQueryObject(HANDLE handle,
                       OBJECT_INFORMATION_CLASS object_information_class,
                       void* object_information,
                       ULONG object_information_length,
                       ULONG* return_length) {
  static decltype(::NtQueryObject)* nt_query_object =
      reinterpret_cast<decltype(::NtQueryObject)*>(
          GetProcAddress(LoadLibrary(L"ntdll.dll"), "NtQueryObject"));
  DCHECK(nt_query_object);
  return nt_query_object(handle,
                         object_information_class,
                         object_information,
                         object_information_length,
                         return_length);
}

// Explicit instantiations with the only 2 valid template arguments to avoid
// putting the body of the function in the header.
template NTSTATUS NtOpenThread<process_types::internal::Traits32>(
    PHANDLE thread_handle,
    ACCESS_MASK desired_access,
    POBJECT_ATTRIBUTES object_attributes,
    const process_types::CLIENT_ID<process_types::internal::Traits32>*
        client_id);

template NTSTATUS NtOpenThread<process_types::internal::Traits64>(
    PHANDLE thread_handle,
    ACCESS_MASK desired_access,
    POBJECT_ATTRIBUTES object_attributes,
    const process_types::CLIENT_ID<process_types::internal::Traits64>*
        client_id);

}  // namespace crashpad
