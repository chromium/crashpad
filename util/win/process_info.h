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

#include <sys/types.h>
#include <windows.h>

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "util/misc/initialization_state_dcheck.h"
#include "util/win/address_types.h"

namespace crashpad {

//! \brief Gathers information about a process given its `HANDLE`. This consists
//!     primarily of information stored in the Process Environment Block.
class ProcessInfo {
 public:
  //! \brief Contains information about a module loaded into a process.
  struct Module {
    Module();
    ~Module();

    //! \brief The pathname used to load the module from disk.
    std::wstring name;

    //! \brief The base address of the loaded DLL.
    WinVMAddress dll_base;

    //! \brief The size of the module.
    WinVMSize size;

    //! \brief The module's timestamp.
    time_t timestamp;
  };

  // \brief Contains information about a range of pages in the virtual address
  //    space of a process.
  struct MemoryInfo {
    explicit MemoryInfo(const MEMORY_BASIC_INFORMATION& mbi);
    ~MemoryInfo();

    //! \brief The base address of the region of pages.
    WinVMAddress base_address;

    //! \brief The size of the region beginning at base_address in bytes.
    WinVMSize region_size;

    //! \brief The base address of a range of pages that was allocated by
    //!     `VirtualAlloc()`. The page pointed to base_address is within this
    //!     range of pages.
    WinVMAddress allocation_base;

    //! \brief The state of the pages, one of `MEM_COMMIT`, `MEM_FREE`, or
    //!     `MEM_RESERVE`.
    uint32_t state;

    //! \brief The memory protection option when this page was originally
    //!     allocated. This will be `PAGE_EXECUTE`, `PAGE_EXECUTE_READ`, etc.
    uint32_t allocation_protect;

    //! \brief The current memoryprotection state. This will be `PAGE_EXECUTE`,
    //!   `PAGE_EXECUTE_READ`, etc.
    uint32_t protect;

    //! \brief The type of the pages. This will be one of `MEM_IMAGE`,
    //!     `MEM_MAPPED`, or `MEM_PRIVATE`.
    uint32_t type;
  };

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

  //! \brief Gets the address and size of the process's Process Environment
  //!     Block.
  //!
  //! \param[out] peb_address The address of the Process Environment Block.
  //! \param[out] peb_size The size of the Process Environment Block.
  void Peb(WinVMAddress* peb_address, WinVMSize* peb_size) const;

  //! \brief Retrieves the modules loaded into the target process.
  //!
  //! The modules are enumerated in initialization order as detailed in the
  //!     Process Environment Block. The main executable will always be the
  //!     first element.
  bool Modules(std::vector<Module>* modules) const;

  //! \brief Retrieves information about all pages mapped into the process.
  const std::vector<MemoryInfo>& MemoryInformation() const;

 private:
  template <class Traits>
  friend bool GetProcessBasicInformation(HANDLE process,
                                         bool is_wow64,
                                         ProcessInfo* process_info,
                                         WinVMAddress* peb_address,
                                         WinVMSize* peb_size);
  template <class Traits>
  friend bool ReadProcessData(HANDLE process,
                              WinVMAddress peb_address_vmaddr,
                              ProcessInfo* process_info);

  friend bool ReadMemoryInfo(HANDLE process,
                             bool is_64_bit,
                             ProcessInfo* process_info);

  pid_t process_id_;
  pid_t inherited_from_process_id_;
  std::wstring command_line_;
  WinVMAddress peb_address_;
  WinVMSize peb_size_;
  std::vector<Module> modules_;
  std::vector<MemoryInfo> memory_info_;
  bool is_64_bit_;
  bool is_wow64_;
  InitializationStateDcheck initialized_;

  DISALLOW_COPY_AND_ASSIGN(ProcessInfo);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_WIN_PROCESS_INFO_H_
