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

#ifndef CRASHPAD_SNAPSHOT_TEST_TEST_PROCESS_SNAPSHOT_H_
#define CRASHPAD_SNAPSHOT_TEST_TEST_PROCESS_SNAPSHOT_H_

#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "snapshot/exception_snapshot.h"
#include "snapshot/process_snapshot.h"
#include "snapshot/system_snapshot.h"
#include "util/stdlib/pointer_container.h"

namespace crashpad {

class ModuleSnapshot;
class ThreadSnapshot;

namespace test {

//! \brief A test ProcessSnapshot that can carry arbitrary data for testing
//!     purposes.
class TestProcessSnapshot final : public ProcessSnapshot {
 public:
  TestProcessSnapshot();
  ~TestProcessSnapshot() override;

  void SetProcessID(pid_t process_id) { process_id_ = process_id; }
  void SetParentProcessID(pid_t parent_process_id) {
    parent_process_id_ = parent_process_id;
  }
  void SetSnapshotTime(const timeval& snapshot_time) {
    snapshot_time_ = snapshot_time;
  }
  void SetProcessStartTime(const timeval& start_time) {
    process_start_time_ = start_time;
  }
  void SetProcessCPUTimes(const timeval& user_time,
                          const timeval& system_time) {
    process_cpu_user_time_ = user_time;
    process_cpu_system_time_ = system_time;
  }

  //! \brief Sets the system snapshot to be returned by System().
  //!
  //! \param[in] system The system snapshot that System() will return. The
  //!     TestProcessSnapshot object takes ownership of \a system.
  void SetSystem(scoped_ptr<SystemSnapshot> system) { system_ = system.Pass(); }

  //! \brief Adds a thread snapshot to be returned by Threads().
  //!
  //! \param[in] thread The thread snapshot that will be included in Threads().
  //!     The TestProcessSnapshot object takes ownership of \a thread.
  void AddThread(scoped_ptr<ThreadSnapshot> thread) {
    threads_.push_back(thread.release());
  }

  //! \brief Adds a module snapshot to be returned by Modules().
  //!
  //! \param[in] module The module snapshot that will be included in Modules().
  //!     The TestProcessSnapshot object takes ownership of \a module.
  void AddModule(scoped_ptr<ModuleSnapshot> module) {
    modules_.push_back(module.release());
  }

  //! \brief Sets the exception snapshot to be returned by Exception().
  //!
  //! \param[in] exception The exception snapshot that Exception() will return.
  //!     The TestProcessSnapshot object takes ownership of \a exception.
  void SetException(scoped_ptr<ExceptionSnapshot> exception) {
    exception_ = exception.Pass();
  }

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
  pid_t process_id_;
  pid_t parent_process_id_;
  timeval snapshot_time_;
  timeval process_start_time_;
  timeval process_cpu_user_time_;
  timeval process_cpu_system_time_;
  scoped_ptr<SystemSnapshot> system_;
  PointerVector<ThreadSnapshot> threads_;
  PointerVector<ModuleSnapshot> modules_;
  scoped_ptr<ExceptionSnapshot> exception_;

  DISALLOW_COPY_AND_ASSIGN(TestProcessSnapshot);
};

}  // namespace test
}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_TEST_TEST_PROCESS_SNAPSHOT_H_
