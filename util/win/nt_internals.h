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

#include <windows.h>
#include <winternl.h>

#include "util/win/process_structs.h"

namespace crashpad {

// Copied from ntstatus.h because um/winnt.h conflicts with general inclusion of
// ntstatus.h.
#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xC0000023L)
#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004L)

// winternal.h defines THREADINFOCLASS, but not all members.
enum { ThreadBasicInformation = 0 };

// winternal.h defines SYSTEM_INFORMATION_CLASS, but not all members.
enum { SystemExtendedHandleInformation = 64 };

NTSTATUS NtQuerySystemInformation(
    SYSTEM_INFORMATION_CLASS system_information_class,
    PVOID system_information,
    ULONG system_information_length,
    PULONG return_length);

NTSTATUS NtQueryInformationThread(HANDLE thread_handle,
                                  THREADINFOCLASS thread_information_class,
                                  PVOID thread_information,
                                  ULONG thread_information_length,
                                  PULONG return_length);

template <class Traits>
NTSTATUS NtOpenThread(PHANDLE thread_handle,
                      ACCESS_MASK desired_access,
                      POBJECT_ATTRIBUTES object_attributes,
                      const process_types::CLIENT_ID<Traits>* client_id);

NTSTATUS NtQueryObject(HANDLE handle,
                       OBJECT_INFORMATION_CLASS object_information_class,
                       void* object_information,
                       ULONG object_information_length,
                       ULONG* return_length);

}  // namespace crashpad
