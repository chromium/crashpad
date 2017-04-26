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
#include <sys/user.h>

#include <memory>

#include "base/macros.h"
#include "util/linux/memory_map.h"
#include "util/linux/process_memory.h"
#include "util/linux/scoped_ptrace_attach.h"
#include "util/posix/process_info.h"
#include "util/misc/initialization_state_dcheck.h"

namespace crashpad {

class ProcessReader {
 public:
  struct Thread {
// TODO(jperaza): sys/user.h only provides register structs for the architecture
// of this process. We will need to provide definitions to trace processes with
// different bitness.
#if defined(ARCH_CPU_ARM64)
    using ThreadContext = user_regs_struct;
    using FloatContext = user_fpsimd_struct;
#define STACK_POINTER(context) context.sp
#elif defined(ARCH_CPU_X86_FAMILY)
    using ThreadContext = user_regs_struct;
    using FloatContext = user_fpregs_struct;
#define STACK_POINTER(context) context.rsp
#endif  // ARCH_CPU_ARM64

    pid_t tid;
    ThreadContext thread_context;
    FloatContext float_context;
    LinuxVMAddress stack_region_address;
    LinuxVMSize stack_region_size;
    LinuxVMAddress thread_specific_data_address;
    int priority;
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

 private:
  void InitializeThreads();
  void PtraceInitializeThreads();
  void SelfInitializeThreads();

  ProcessInfo process_info_;
  MemoryMap memory_map_;
  std::vector<Thread> threads_;
  std::unique_ptr<ProcessMemory> process_memory_;
  InitializationStateDcheck initialized_;

  bool is_64_bit_;
  bool initialized_threads_;

  DISALLOW_COPY_AND_ASSIGN(ProcessReader);
};

}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_LINUX_PROCESS_READER_H_
