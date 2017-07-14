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

#ifndef CRASHPAD_SNAPSHOT_LINUX_PROCESS_READER_H_
#define CRASHPAD_SNAPSHOT_LINUX_PROCESS_READER_H_

#include <sys/types.h>

#include <memory>

#include "base/macros.h"
#include "util/linux/address_types.h"
#include "util/linux/memory_map.h"
#include "util/linux/process_memory.h"
#include "util/linux/thread_info.h"
#include "util/linux/scoped_ptrace_attach.h"
#include "util/posix/process_info.h"
#include "util/misc/initialization_state.h"
#include "util/misc/initialization_state_dcheck.h"
#include "util/stdlib/pointer_container.h"

namespace crashpad {

class ElfImageReader;

//! \brief Accesses information about another process, identified by a process
//!     ID.
class ProcessReader {
 public:
  //! \brief Contains information about a thread that belongs to a process.
  struct Thread {
    Thread();
    ~Thread();

    ThreadInfo thread_info;
    ThreadContext thread_context;
    FloatContext float_context;
    LinuxVMAddress stack_region_address;
    LinuxVMSize stack_region_size;
    LinuxVMAddress thread_specific_data_address;
    pid_t tid;
    InitializationState initialized;
    int sched_policy;
    int static_priority;
    int nice_value;
    int suspend_count;
  };

  //! \brief Contains information about a module loaded into a process.
  struct Module {
    Module();
    ~Module();

    //! \brief The pathname used to load the module from disk.
    std::string name;

    //! \brief An image reader for the module.
    //!
    //! The lifetime of this ElfImageReader is scoped to the lifefime of the
    //! ProcessReader that created it.
    //!
    //! This field may be `nullptr` if a reader could not be created for the
    //! module.
    const ElfImageReader* reader;
  };

  ProcessReader();
  ~ProcessReader();

  //! \brief Initializes this object. This method must be called before any
  //!     other.
  //!
  //! \param[in] pid The process ID of the target process.
  //!
  //! \return `true` on success, indicating that this object will respond
  //!     validly to further method calls. `false` on failure. On failure, no
  //!     further method calls should be made.
  bool Initialize(pid_t pid);

  //! \return `true` if the target task is a 64-bit process.
  bool Is64Bit() const { return is_64_bit_; }

  //! \return The target process' process ID.
  pid_t ProcessID() const { return process_info_.ProcessID(); }

  //! \return The target process' parent process ID.
  pid_t ParentProcessID() const { return process_info_.ParentProcessID(); }

  //! \return Accesses the memory of the target process.
  ProcessMemory* Memory() { return process_memory_.get(); }

  //! \return The threads that are in the task process. The first element (at
  //!     index `0`) corresponds to the main thread.
  const std::vector<Thread>& Threads();

  //! \return The modules loaded in the process. The first element (at index
  //!     `0`) corresponds to the main executable.
  const std::vector<Module>& Modules();

 private:
  void InitializeThreads();
  void PtraceInitializeThreads();
  void InitializeModules();

  ProcessInfo process_info_;
  MemoryMap memory_map_;
  std::vector<Thread> threads_;
  std::vector<Module> modules_;
  std::unique_ptr<ProcessMemory> process_memory_;
  PointerVector<ElfImageReader> module_readers_;
  InitializationStateDcheck initialized_;

  bool is_64_bit_;
  bool initialized_threads_;

  DISALLOW_COPY_AND_ASSIGN(ProcessReader);
};

}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_LINUX_PROCESS_READER_H_
