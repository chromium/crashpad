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

#include <sys/time.h>
#include <sys/types.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "util/linux/address_types.h"
#include "util/linux/memory_map.h"
#include "util/linux/process_memory.h"
#include "util/linux/thread_info.h"
#include "util/posix/process_info.h"
#include "util/misc/initialization_state_dcheck.h"

namespace crashpad {

//! \brief Accesses information about another process, identified by a process
//!     ID.
class ProcessReader {
 public:
  //! \brief Contains information about a thread that belongs to a process.
  struct Thread {
    Thread();
    ~Thread();

    ThreadContext thread_context;
    FloatContext float_context;
    LinuxVMAddress thread_specific_data_address;
    LinuxVMAddress stack_region_address;
    LinuxVMSize stack_region_size;
    pid_t tid;
    int sched_policy;
    int static_priority;
    int nice_value;

   private:
    friend class ProcessReader;

    bool InitializePtrace();
    void InitializeStack(ProcessReader* reader);
  };

  ProcessReader();
  ~ProcessReader();

  //! \brief Initializes this object.
  //!
  //! This method must be successfully called before calling any other method in
  //! this class.
  //!
  //! \param[in] pid The process ID of the target process.
  //! \return `true` on success. `false` on failure with a message logged.
  bool Initialize(pid_t pid);

  //! \brief Return `true` if the target task is a 64-bit process.
  bool Is64Bit() const { return is_64_bit_; }

  //! \brief Return the target process' process ID.
  pid_t ProcessID() const { return process_info_.ProcessID(); }

  //! \brief Return the target process' parent process ID.
  pid_t ParentProcessID() const { return process_info_.ParentProcessID(); }

  //! \brief Return a memory reader for the target process.
  ProcessMemory* Memory() { return process_memory_.get(); }

  //! \brief Return a memory map of the target process.
  MemoryMap* GetMemoryMap() { return &memory_map_; }

  //! \brief Determines the target process’ start time.
  //!
  //! \param[out] start_time The time that the process started.
  //! \return `true` on success with \a start_time set. Otherwise `false` with a
  //!     message logged.
  bool StartTime(timeval* start_time) const;

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

  //! \brief Return a vector of threads that are in the task process. If the
  //!     main thread is able to be identified and traced, it will be placed at
  //!     index `0`.
  const std::vector<Thread>& Threads();

 private:
  void InitializeThreads();

  ProcessInfo process_info_;
  class MemoryMap memory_map_;
  std::vector<Thread> threads_;
  std::unique_ptr<ProcessMemory> process_memory_;
  bool is_64_bit_;
  bool initialized_threads_;
  InitializationStateDcheck initialized_;

  DISALLOW_COPY_AND_ASSIGN(ProcessReader);
};

}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_LINUX_PROCESS_READER_H_
