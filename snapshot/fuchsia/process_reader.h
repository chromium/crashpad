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

#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "snapshot/elf/elf_image_reader.h"
#include "util/misc/initialization_state_dcheck.h"
#include "util/process/process_memory_fuchsia.h"

namespace crashpad {

//! \brief Accesses information about another process, identified by a Fuchsia
//!     process.
class ProcessReader {
 public:
  //! \brief Contains information about a module loaded into a process.
  struct Module {
    Module();
    ~Module();

    //! \brief The pathname used to load the module from disk.
    std::string name;

    //! \brief An image reader for the module.
    //! XXX
    //! XXX
    ElfImageReader* reader;

    //! \brief The module’s timestamp.
    //! XXX
    //! XXX
    time_t timestamp;
  };

  ProcessReader();
  ~ProcessReader();

  //! XXX
  bool Initialize(zx_handle_t process);

  //! XXX
  const std::vector<Module>& Modules();

 private:
  void InitializeModules();

  std::vector<Module> modules_;
  std::vector<std::unique_ptr<ElfImageReader>> module_readers_;
  std::unique_ptr<ProcessMemoryFuchsia> process_memory_;
  zx_handle_t process_;
  bool initialized_modules_ = false;
  InitializationStateDcheck initialized_;

  DISALLOW_COPY_AND_ASSIGN(ProcessReader);
};

}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_FUCHSIA_PROCESS_READER_H_
