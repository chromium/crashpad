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

#ifndef CRASHPAD_SNAPSHOT_PROCESS_SNAPSHOT_H_
#define CRASHPAD_SNAPSHOT_PROCESS_SNAPSHOT_H_

#include <sys/time.h>
#include <sys/types.h>

#include <vector>

namespace crashpad {

class ExceptionSnapshot;
class ModuleSnapshot;
class SystemSnapshot;
class ThreadSnapshot;

//! \brief An abstract interface to a snapshot representing the state of a
//!     process.
//!
//! This is the top-level object in a family of Snapshot objects, because it
//! gives access to a SystemSnapshot, vectors of ModuleSnapshot and
//! ThreadSnapshot objects, and possibly an ExceptionSnapshot. In turn,
//! ThreadSnapshot and ExceptionSnapshot objects both give access to CPUContext
//! objects, and ThreadSnapshot objects also give access to MemorySnapshot
//! objects corresponding to thread stacks.
class ProcessSnapshot {
 public:
  virtual ~ProcessSnapshot() {}

  //! \brief Returns the snapshot process’ process ID.
  virtual pid_t ProcessID() const = 0;

  //! \brief Returns the snapshot process’ parent process’ process ID.
  virtual pid_t ParentProcessID() const = 0;

  //! \brief Returns the time that the snapshot was taken in \a snapshot_time.
  //!
  //! \param[out] snapshot_time The time that the snapshot was taken. This is
  //!     distinct from the time that a ProcessSnapshot object was created or
  //!     initialized, although it may be that time for ProcessSnapshot objects
  //!     representing live or recently-crashed process state.
  virtual void SnapshotTime(timeval* snapshot_time) const = 0;

  //! \brief Returns the time that the snapshot process was started in \a
  //!     start_time.
  //!
  //! Normally, process uptime in wall clock time can be computed as
  //! SnapshotTime() − ProcessStartTime(), but this cannot be guaranteed in
  //! cases where the real-time clock has been set during the snapshot process’
  //! lifetime.
  //!
  //! \param[out] start_time The time that the process was started.
  virtual void ProcessStartTime(timeval* start_time) const = 0;

  //! \brief Returns the snapshot process’ CPU usage times in \a user_time and
  //!     \a system_time.
  //!
  //! \param[out] user_time The time that the process has spent executing in
  //!     user mode.
  //! \param[out] system_time The time that the process has spent executing in
  //!     system (kernel) mode.
  virtual void ProcessCPUTimes(timeval* user_time,
                               timeval* system_time) const = 0;

  //! \brief Returns a SystemSnapshot reflecting the characteristics of the
  //!     system that ran the snapshot process at the time of the snapshot.
  //!
  //! \return A SystemSnapshot object. The caller does not take ownership of
  //!     this object, it is scoped to the lifetime of the ProcessSnapshot
  //!     object that it was obtained from.
  virtual const SystemSnapshot* System() const = 0;

  //! \brief Returns ModuleSnapshot objects reflecting the code modules (binary
  //!     images) loaded into the snapshot process at the time of the snapshot.
  //!
  //! \return A vector of ModuleSnapshot objects. The caller does not take
  //!     ownership of these objects, they are scoped to the lifetime of the
  //!     ProcessSnapshot object that they were obtained from.
  virtual std::vector<const ModuleSnapshot*> Modules() const = 0;

  //! \brief Returns ThreadSnapshot objects reflecting the threads (lightweight
  //!     processes) existing in the snapshot process at the time of the
  //!     snapshot.
  //!
  //! \return A vector of ThreadSnapshot objects. The caller does not take
  //!     ownership of these objects, they are scoped to the lifetime of the
  //!     ProcessSnapshot object that they were obtained from.
  virtual std::vector<const ThreadSnapshot*> Threads() const = 0;

  //! \brief Returns an ExceptionSnapshot reflecting the exception that the
  //!     snapshot process sustained to trigger the snapshot being taken.
  //!
  //! \return An ExceptionSnapshot object. The caller does not take ownership of
  //!     this object, it is scoped to the lifetime of the ProcessSnapshot
  //!     object that it was obtained from. If the snapshot is not a result of
  //!     an exception, returns `nullptr`.
  virtual const ExceptionSnapshot* Exception() const = 0;
};

}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_PROCESS_SNAPSHOT_H_
