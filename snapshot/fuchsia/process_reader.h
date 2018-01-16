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

#include "snapshot/elf/elf_image_reader.h"
#include "util/misc/initialization_state_dcheck.h"

namespace crashpad {

//! \brief Accesses information about another process, identified by a process
//!     handle.
class ProcessReader {
 public:
  //! \brief Contains information about a thread that belongs to a process.
  struct Thread {
    Thread();
    ~Thread();

   private:
    friend class ProcessReader;
  };

  //! \brief Contains information about a module loaded into a process.
  struct Module {
    Module();
    ~Module();

    //! \brief The `ZX_PROP_NAME` value for the module.
    std::string name;

    //! \brief An image reader for the module.
    //!
    //! The lifetime of this ElfImageReader is scoped to the lifetime of the
    //! ProcessReader that created it.
    //!
    //! This field may be `nullptr` if a reader could not be created for the
    //! module.
    ElfImageReader* elf_reader;

    //! \brief The module's type.
    ModuleSnapshot::ModuleType type;
  };

  ProcessReader();
  ~ProcessReader();

  //! \brief Initializes this object.
  //!
  //! This method must be successfully called before calling any other method in
  //! this class and may only be called once.
  //!
  //! XXX
  bool Initialize(zx_handle_t process);

  //! \brief Return a memory reader for the target process.
  ProcessMemory* Memory() { return &process_memory_; }

  //! \brief Return a memory map of the target process.
  MemoryMap* GetMemoryMap() { return &memory_map_; }

  //! \brief Return a vector of threads that are in the task process. If the
  //!     main thread is able to be identified and traced, it will be placed at
  //!     index `0`.
  const std::vector<Thread>& Threads();

  //! \return The modules loaded in the process. The first element (at index
  //!     `0`) corresponds to the main executable.
  const std::vector<Module>& Modules();

 private:
  void InitializeThreads();
  void InitializeModules();

  ProcessInfo process_info_;
  MemoryMap memory_map_;
  std::vector<Thread> threads_;
  std::vector<Module> modules_;
  std::vector<std::unique_ptr<ElfImageReader>> elf_readers_;
  ProcessMemoryFuchsia process_memory_;
  bool initialized_threads_;
  bool initialized_modules_;
  InitializationStateDcheck initialized_;

  DISALLOW_COPY_AND_ASSIGN(ProcessReader);
};

}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_FUCHSIA_PROCESS_READER_H_
