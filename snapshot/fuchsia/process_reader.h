// Copyright 2018 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_SNAPSHOT_FUCHSIA_PROCESS_READER_H_
#define CRASHPAD_SNAPSHOT_FUCHSIA_PROCESS_READER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "snapshot/elf/elf_image_reader.h"
#include "util/misc/initialization_state_dcheck.h"
#include "util/process/process_memory_fuchsia.h"
#include "util/process/process_memory_range.h"

namespace crashpad {

//! \brief Accesses information about another process, identified by a Fuchsia
//!     process.
class ProcessReader {
 public:
  //! \brief Contains information about a module loaded into a process.
  struct Module {
    Module();
    ~Module();

    //! \brief The `ZX_PROP_NAME` of the module. Will be prepended with "app:"
    //!     for the main executable.
    std::string name;

    //! \brief An image reader for the module.
    //!
    //! The lifetime of this ElfImageReader is scoped to the lifetime of the
    //! ProcessReader that created it.
    //!
    //! This field may be `nullptr` if a reader could not be created for the
    //! module.
    ElfImageReader* reader;
  };

  ProcessReader();
  ~ProcessReader();

  //! \brief Initializes this object. This method must be called before any
  //!     other.
  //!
  //! \param[in] process A process handle with permissions to read properties
  //!     and memory from the target process.
  //!
  //! \return `true` on success, indicating that this object will respond
  //!     validly to further method calls. `false` on failure. On failure, no
  //!     further method calls should be made.
  bool Initialize(zx_handle_t process);

  //! \return The modules loaded in the process. The first element (at index
  //!     `0`) corresponds to the main executable.
  const std::vector<Module>& Modules();

 private:
  void InitializeModules();

  std::vector<Module> modules_;
  std::vector<std::unique_ptr<ElfImageReader>> module_readers_;
  std::vector<std::unique_ptr<ProcessMemoryRange>> process_memory_ranges_;
  std::unique_ptr<ProcessMemoryFuchsia> process_memory_;
  zx_handle_t process_;
  bool initialized_modules_ = false;
  InitializationStateDcheck initialized_;

  DISALLOW_COPY_AND_ASSIGN(ProcessReader);
};

}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_FUCHSIA_PROCESS_READER_H_
