// Copyright 2017 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_SNAPSHOT_LINUX_DEBUG_RENDEZVOUS_H_
#define CRASHPAD_SNAPSHOT_LINUX_DEBUG_RENDEZVOUS_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "util/linux/address_types.h"
#include "util/linux/process_memory.h"

namespace crashpad {

//! \brief Reads the Rendezvous structure defined in `<link.h>` via a
//!     ProcessMemory.
class DebugRendezvous {
 public:
  //! \brief An entry in the dynamic linker's list of loaded shared objects.
  struct LinkEntry {
    LinkEntry();

    //! \brief A filename identifying the object.
    std::string name;

    //! \brief The difference between the preferred load address in the ELF file
    //!     and the actual loaded address in memory.
    LinuxVMOffset load_bias;

    //! \brief The address of the dynamic array for this module.
    LinuxVMAddress dynamic_array;
  };

  DebugRendezvous();
  ~DebugRendezvous();

  //! \brief Initializes this object by reading an r_debug struct from a target
  //!     process.
  //!
  //! This method must be called successfully prior to calling any other method
  //! in this class.
  //!
  //! \param[in] memory A memory reader for the remote process.
  //! \param[in] address The address of an r_debug structure in the remote
  //!     process.
  //! \param[in] is_64_bit Whether the target process is 64-bit.
  //! \return `true` on success. `false` on failure with a message logged.
  bool Initialize(const ProcessMemory& memory,
                  LinuxVMAddress address,
                  bool is_64_bit);

  //! \brief Returns the LinkEntry for the main executable.
  const LinkEntry* Executable() const { return &executable_; }

  //! \brief Returns the a vector of LinkEntry for the modules read from the
  //!     link map.
  const std::vector<LinkEntry>& Modules() const { return modules_; }

 private:
  template <typename Traits>
  bool InitializeSpecific(const ProcessMemory& memory, LinuxVMAddress address);

  std::vector<LinkEntry> modules_;
  LinkEntry executable_;

  DISALLOW_COPY_AND_ASSIGN(DebugRendezvous);
};

}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_LINUX_DEBUG_RENDEZVOUS_H_
