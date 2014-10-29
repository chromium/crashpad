// Copyright 2014 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_SNAPSHOT_MAC_PROCESS_SNAPSHOT_MAC_H_
#define CRASHPAD_SNAPSHOT_MAC_PROCESS_SNAPSHOT_MAC_H_

#include <mach/mach.h>
#include <sys/time.h>
#include <unistd.h>

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "snapshot/exception_snapshot.h"
#include "snapshot/mac/exception_snapshot_mac.h"
#include "snapshot/mac/module_snapshot_mac.h"
#include "snapshot/mac/process_reader.h"
#include "snapshot/mac/system_snapshot_mac.h"
#include "snapshot/mac/thread_snapshot_mac.h"
#include "snapshot/module_snapshot.h"
#include "snapshot/process_snapshot.h"
#include "snapshot/system_snapshot.h"
#include "snapshot/thread_snapshot.h"
#include "util/misc/initialization_state_dcheck.h"
#include "util/stdlib/pointer_container.h"

namespace crashpad {

//! \brief A ProcessSnapshot of a running (or crashed) process running on a Mac
//!     OS X system.
class ProcessSnapshotMac final : public ProcessSnapshot {
 public:
  ProcessSnapshotMac();
  ~ProcessSnapshotMac() override;

  //! \brief Initializes the object.
  //!
  //! \param[in] task The task to create a snapshot from.
  //!
  //! \return `true` if the snapshot could be created, `false` otherwise with
  //!     an appropriate message logged.
  bool Initialize(task_t task);

  //! \brief Initializes the object’s exception.
  //!
  //! This populates the data to be returned by Exception(). The parameters may
  //! be passed directly through from a Mach exception handler.
  //!
  //! This method must not be called until after a successful call to
  //! Initialize().
  //!
  //! \return `true` if the exception information could be initialized, `false`
  //!     otherwise with an appropriate message logged. When this method returns
  //!     `false`, the ProcessSnapshotMac object’s validity remains unchanged.
  bool InitializeException(thread_t exception_thread,
                           exception_type_t exception,
                           const mach_exception_data_type_t* code,
                           mach_msg_type_number_t code_count,
                           thread_state_flavor_t flavor,
                           const natural_t* state,
                           mach_msg_type_number_t state_count);

  // ProcessSnapshot:

  pid_t ProcessID() const override;
  pid_t ParentProcessID() const override;
  void SnapshotTime(timeval* snapshot_time) const override;
  void ProcessStartTime(timeval* start_time) const override;
  void ProcessCPUTimes(timeval* user_time, timeval* system_time) const override;
  const SystemSnapshot* System() const override;
  std::vector<const ThreadSnapshot*> Threads() const override;
  std::vector<const ModuleSnapshot*> Modules() const override;
  const ExceptionSnapshot* Exception() const override;

 private:
  // Initializes threads_ on behalf of Initialize().
  void InitializeThreads();

  // Initializes modules_ on behalf of Initialize().
  void InitializeModules();

  internal::SystemSnapshotMac system_;
  PointerVector<internal::ThreadSnapshotMac> threads_;
  PointerVector<internal::ModuleSnapshotMac> modules_;
  scoped_ptr<internal::ExceptionSnapshotMac> exception_;
  ProcessReader process_reader_;
  timeval snapshot_time_;
  InitializationStateDcheck initialized_;

  DISALLOW_COPY_AND_ASSIGN(ProcessSnapshotMac);
};

}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_MAC_PROCESS_SNAPSHOT_MAC_H_
