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

#ifndef CRASHPAD_SNAPSHOT_IOS_PROCESS_SNAPSHOT_IOS_H_
#define CRASHPAD_SNAPSHOT_IOS_PROCESS_SNAPSHOT_IOS_H_

// #include <mach/mach.h>
// #include <sys/time.h>
// #include <unistd.h>

// #include <map>
// #include <memory>
// #include <string>
#include <vector>

// #include "base/macros.h"
// #include "client/crashpad_info.h"
// #include "snapshot/crashpad_info_client_options.h"
// #include "snapshot/exception_snapshot.h"
// #include "snapshot/ios/exception_snapshot_ios.h"
#include "snapshot/ios/module_snapshot_ios.h"
//#include "snapshot/ios/process_reader_ios.h"
#include "snapshot/ios/system_snapshot_ios.h"
#include "util/posix/process_info.h"
// #include "snapshot/ios/thread_snapshot_ios.h"
// #include "snapshot/memory_map_region_snapshot.h"
// #include "snapshot/module_snapshot.h"
#include "snapshot/process_snapshot.h"
// #include "snapshot/system_snapshot.h"
// #include "snapshot/thread_snapshot.h"
#include "snapshot/unloaded_module_snapshot.h"
// #include "util/mach/mach_extensions.h"
// #include "util/misc/initialization_state_dcheck.h"
// #include "util/misc/uuid.h"

namespace crashpad {

//! \brief A ProcessSnapshot of a running (or crashed) process running on a
//!     iphoneOS system.
class ProcessSnapshotIOS final : public ProcessSnapshot {
 public:
  ProcessSnapshotIOS();
  ~ProcessSnapshotIOS() override;

  //! \brief Initializes the object.
  //!
  //! \return `true` if the snapshot could be created, `false` otherwise with
  //!     an appropriate message logged.
  bool Initialize();

  // ProcessSnapshot:
  pid_t ProcessID() const override;
  pid_t ParentProcessID() const override;
  void SnapshotTime(timeval* snapshot_time) const override;
  void ProcessStartTime(timeval* start_time) const override;
  void ProcessCPUTimes(timeval* user_time, timeval* system_time) const override;
  void ReportID(UUID* report_id) const override;
  void ClientID(UUID* client_id) const override;
  const std::map<std::string, std::string>& AnnotationsSimpleMap()
      const override;
  const SystemSnapshot* System() const override;
  std::vector<const ThreadSnapshot*> Threads() const override;
  std::vector<const ModuleSnapshot*> Modules() const override;
  std::vector<UnloadedModuleSnapshot> UnloadedModules() const override;
  const ExceptionSnapshot* Exception() const override;
  std::vector<const MemoryMapRegionSnapshot*> MemoryMap() const override;
  std::vector<HandleSnapshot> Handles() const override;
  std::vector<const MemorySnapshot*> ExtraMemory() const override;
  const ProcessMemory* Memory() const override;

 private:
  // Initializes threads_ on behalf of Initialize().
  void InitializeThreads();

  // Initializes modules_ on behalf of Initialize().
  void InitializeModules();

  mach_vm_address_t DyldAllImageInfo(mach_vm_size_t* all_image_info_size);

  ProcessInfo process_info_;
  internal::SystemSnapshotIOS system_;
  //  std::vector<std::unique_ptr<internal::ThreadSnapshotIOS>> threads_;
  std::vector<std::unique_ptr<internal::ModuleSnapshotIOS>> modules_;
  // std::unique_ptr<internal::ExceptionSnapshotIOS> exception_;
  UUID report_id_;
  UUID client_id_;
  std::map<std::string, std::string> annotations_simple_map_;
  timeval snapshot_time_;
  InitializationStateDcheck initialized_;

  DISALLOW_COPY_AND_ASSIGN(ProcessSnapshotIOS);
};

}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_IOS_PROCESS_SNAPSHOT_IOS_H_
