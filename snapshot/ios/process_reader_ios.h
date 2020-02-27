// Copyright 2020 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_SNAPSHOT_IOS_PROCESS_READER_IOS_H_
#define CRASHPAD_SNAPSHOT_IOS_PROCESS_READER_IOS_H_

#include <mach/mach.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "build/build_config.h"
#include "util/misc/initialization_state_dcheck.h"
#include "util/posix/process_info.h"

namespace crashpad {

class MachOImageReader;

//! \brief Accesses information about this process
class ProcessReaderIOS {
 public:
  //! \brief Contains information about a thread that belongs to a task
  //!     (process).
  struct Thread {
#if defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARM_FAMILY)
    union ThreadContext {
      x86_thread_state64_t t64;
      x86_thread_state32_t t32;
    };
    union FloatContext {
      x86_float_state64_t f64;
      x86_float_state32_t f32;
    };
    union DebugContext {
      x86_debug_state64_t d64;
      x86_debug_state32_t d32;
    };
#endif

    Thread();
    ~Thread() {}

    ThreadContext thread_context;
    FloatContext float_context;
    DebugContext debug_context;
    uint64_t id;
    mach_vm_address_t stack_region_address;
    mach_vm_size_t stack_region_size;
    mach_vm_address_t thread_specific_data_address;
    thread_t port;
    int suspend_count;
    int priority;
  };

  //! \brief Contains information about a module loaded into a process.
  struct Module {
    Module();
    ~Module();

    //! \brief The pathname used to load the module from disk.
    std::string name;

    //! \brief The module’s timestamp.
    //!
    //! This field will be `0` if its value cannot be determined. It can only be
    //! determined for images that are loaded by dyld, so it will be `0` for the
    //! main executable and for dyld itself.
    time_t timestamp;
  };

  ProcessReaderIOS();
  ~ProcessReaderIOS();

  //! \brief Initializes this object. This method must be called before any
  //!     other.
  //!
  //! \return `true` on success, indicating that this object will respond
  //!     validly to further method calls. `false` on failure. On failure, no
  //!     further method calls should be made.
  bool Initialize();

  //! \return `true` if the target task is a 64-bit process.
  bool Is64Bit() const { return is_64_bit_; }

  //! \return The target task’s process ID.
  pid_t ProcessID() const { return process_info_.ProcessID(); }

  //! \return The target task’s parent process ID.
  pid_t ParentProcessID() const { return process_info_.ParentProcessID(); }

  //! \brief Determines the target process’ start time.
  //!
  //! \param[out] start_time The time that the process started.
  void StartTime(timeval* start_time) const;

  //! \brief Determines the target process’ execution time.
  //!
  //! \param[out] user_time The amount of time the process has executed code in
  //!     user mode.
  //! \param[out] system_time The amount of time the process has executed code
  //!     in system mode.
  //!
  //! \return `true` on success, `false` on failure, with a warning logged. On
  //!     failure, \a user_time and \a system_time will be set to represent no
  //!     time spent executing code in user or system mode.
  bool CPUTimes(timeval* user_time, timeval* system_time) const;

  //! \return The threads that are in the task (process). The first element (at
  //!     index `0`) corresponds to the main thread.
  const std::vector<Thread>& Threads();

  //! \return The modules loaded in the process. The first element (at index
  //!     `0`) corresponds to the main executable, and the final element
  //!     corresponds to the dynamic loader, dyld.
  const std::vector<Module>& Modules();

 private:
  //! Performs lazy initialization of the \a threads_ vector on behalf of
  //! Threads().
  void InitializeThreads();

  //! Performs lazy initialization of the \a modules_ vector on behalf of
  //! Modules().
  void InitializeModules();

  mach_vm_address_t DyldAllImageInfo(mach_vm_size_t* all_image_info_size);

  ProcessInfo process_info_;
  std::vector<Thread> threads_;  // owns send rights
  std::vector<Module> modules_;
  InitializationStateDcheck initialized_;

  // This shadows a method of process_info_, but it’s accessed so frequently
  // that it’s given a first-class field to save a call and a few bit operations
  // on each access.
  bool is_64_bit_;

  bool initialized_threads_;
  bool initialized_modules_;

  DISALLOW_COPY_AND_ASSIGN(ProcessReaderIOS);
};

}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_IOS_PROCESS_READER_IOS_H_
