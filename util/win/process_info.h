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

#ifndef CRASHPAD_UTIL_WIN_PROCESS_INFO_H_
#define CRASHPAD_UTIL_WIN_PROCESS_INFO_H_

#include <basetsd.h>
#include <sys/types.h>
#include <windows.h>
#include <winternl.h>

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "util/misc/initialization_state_dcheck.h"

namespace crashpad {

namespace internal {

//! \brief This structure matches PROCESS_BASIC_INFORMATION in winternl.h but
//!     gives names to the Reserved fields (matching the WDK's ntddk.h).
struct FULL_PROCESS_BASIC_INFORMATION {
  NTSTATUS ExitStatus;
  PPEB PebBaseAddress;
  KAFFINITY AffinityMask;
  PVOID BasePriority;
  ULONG UniqueProcessId;
  ULONG InheritedFromUniqueProcessId;
};

}  // namespace internal

//! \brief Gathers information about a process given its `HANDLE`. This consists
//!     primarily of information stored in the Process Environment Block.
class ProcessInfo {
 public:
  ProcessInfo();
  ~ProcessInfo();

  //! \brief Initializes this object with information about the given
  //!     \a process.
  //!
  //! This method must be called successfully prior to calling any other
  //! method in this class. This method may only be called once.
  //!
  //! \return `true` on success, `false` on failure with a message logged.
  bool Initialize(HANDLE process);

  //! \return `true` if the target process is a 64-bit process.
  bool Is64Bit() const;

  //! \return `true` if the target process is running on the Win32-on-Win64
  //!     subsystem.
  bool IsWow64() const;

  //! \return The target process's process ID.
  pid_t ProcessID() const;

  //! \return The target process's parent process ID.
  pid_t ParentProcessID() const;

  //! \return The command line from the target process's Process Environment
  //!     Block.
  bool CommandLine(std::wstring* command_line) const;

  //! \brief Retrieves the modules loaded into the target process.
  //!
  //! The modules are enumerated in initialization order as detailed in the
  //!     Process Environment Block. The main executable will always be the
  //!     first element.
  bool Modules(std::vector<std::wstring>* modules) const;

 private:
  internal::FULL_PROCESS_BASIC_INFORMATION process_basic_information_;
  std::wstring command_line_;
  std::vector<std::wstring> modules_;
  bool is_64_bit_;
  bool is_wow64_;
  InitializationStateDcheck initialized_;

  DISALLOW_COPY_AND_ASSIGN(ProcessInfo);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_WIN_PROCESS_INFO_H_
