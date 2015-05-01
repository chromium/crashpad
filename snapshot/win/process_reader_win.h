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

#ifndef CRASHPAD_SNAPSHOT_WIN_PROCESS_READER_WIN_H_
#define CRASHPAD_SNAPSHOT_WIN_PROCESS_READER_WIN_H_

#include <windows.h>

#include "util/misc/initialization_state_dcheck.h"
#include "util/win/address_types.h"
#include "util/win/process_info.h"

namespace crashpad {

//! \brief Accesses information about another process, identified by a HANDLE.
class ProcessReaderWin {
 public:
  ProcessReaderWin();
  ~ProcessReaderWin();

  //! \brief Initializes this object. This method must be called before any
  //!     other.
  //!
  //! \param[in] process Process handle, must have PROCESS_QUERY_INFORMATION,
  //!     PROCESS_VM_READ, and PROCESS_DUP_HANDLE access.
  //!
  //! \return `true` on success, indicating that this object will respond
  //!     validly to further method calls. `false` on failure. On failure, no
  //!     further method calls should be made.
  bool Initialize(HANDLE process);

  //! \return `true` if the target task is a 64-bit process.
  bool Is64Bit() const { return process_info_.Is64Bit(); }

  pid_t ProcessID() const { return process_info_.ProcessID(); }
  pid_t ParentProcessID() const { return process_info_.ParentProcessID(); }

  bool ReadMemory(WinVMAddress at, WinVMSize num_bytes, void* into);

  //! \return The modules loaded in the process. The first element (at index
  //!     `0`) corresponds to the main executable.
  const std::vector<ProcessInfo::Module>& Modules();

 private:
  HANDLE process_;
  ProcessInfo process_info_;
  std::vector<ProcessInfo::Module> modules_;
  InitializationStateDcheck initialized_;

  DISALLOW_COPY_AND_ASSIGN(ProcessReaderWin);
};

}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_WIN_PROCESS_READER_WIN_H_
