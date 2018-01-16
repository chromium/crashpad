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

#include "snapshot/fuchsia/process_snapshot_fuchsia.h"

#include <link.h>
#include <zircon/syscalls.h>
#include <zircon/syscalls/debug.h>
#include <zircon/syscalls/object.h>

#include <memory>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"

namespace crashpad {

namespace {

zx_status_t ReadMemory(zx_handle_t process,
                       zx_vaddr_t vaddr,
                       void* ptr,
                       size_t len) {
  size_t actual;
  zx_status_t status =
      zx_process_read_memory(process, vaddr, ptr, len, &actual);
  if (status < 0)
    return status;
  if (len != actual) {
    return ZX_ERR_UNAVAILABLE;
  }
  return ZX_OK;
}

zx_status_t ReadString(zx_handle_t process,
                       zx_vaddr_t vaddr,
                       char* ptr,
                       size_t max) {
  while (max > 1) {
    zx_status_t status;
    if ((status = ReadMemory(process, vaddr, ptr, 1)) < 0) {
      *ptr = 0;
      return status;
    }
    ptr++;
    vaddr++;
    max--;
  }
  *ptr = 0;
  return ZX_OK;
}

void ReadBuildID(zx_handle_t process,
                 zx_vaddr_t base,
                 char* buildid,
                 size_t buildid_len) {
  // TODO(scottmg)
}

}  // namespace

ProcessSnapshotFuchsia::ProcessSnapshotFuchsia()
    : process_(ZX_HANDLE_INVALID) {}

ProcessSnapshotFuchsia::~ProcessSnapshotFuchsia() {}

bool ProcessSnapshotFuchsia::Initialize(zx_handle_t process) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  process_ = process;

  if (gettimeofday(&snapshot_time_, nullptr) != 0) {
    PLOG(ERROR) << "gettimeofday";
    return false;
  }

  InitializeThreads();
  InitializeModules();

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

void ProcessSnapshotFuchsia::GetCrashpadOptions(
    CrashpadInfoClientOptions* options) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  CrashpadInfoClientOptions local_options;

  for (const auto& module : modules_) {
    CrashpadInfoClientOptions module_options;
    module->GetCrashpadOptions(&module_options);

    if (local_options.crashpad_handler_behavior == TriState::kUnset) {
      local_options.crashpad_handler_behavior =
          module_options.crashpad_handler_behavior;
    }
    if (local_options.system_crash_reporter_forwarding == TriState::kUnset) {
      local_options.system_crash_reporter_forwarding =
          module_options.system_crash_reporter_forwarding;
    }
    if (local_options.gather_indirectly_referenced_memory == TriState::kUnset) {
      local_options.gather_indirectly_referenced_memory =
          module_options.gather_indirectly_referenced_memory;
      local_options.indirectly_referenced_memory_cap =
          module_options.indirectly_referenced_memory_cap;
    }

    // If non-default values have been found for all options, the loop can end
    // early.
    if (local_options.crashpad_handler_behavior != TriState::kUnset &&
        local_options.system_crash_reporter_forwarding != TriState::kUnset &&
        local_options.gather_indirectly_referenced_memory != TriState::kUnset) {
      break;
    }
  }

  *options = local_options;
}

pid_t ProcessSnapshotFuchsia::ProcessID() const {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
  return 0;
}

pid_t ProcessSnapshotFuchsia::ParentProcessID() const {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
  return 0;
}

void ProcessSnapshotFuchsia::SnapshotTime(timeval* snapshot_time) const {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
}

void ProcessSnapshotFuchsia::ProcessStartTime(timeval* start_time) const {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
}

void ProcessSnapshotFuchsia::ProcessCPUTimes(timeval* user_time,
                                             timeval* system_time) const {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
}

void ProcessSnapshotFuchsia::ReportID(UUID* report_id) const {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
}

void ProcessSnapshotFuchsia::ClientID(UUID* client_id) const {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
}

const std::map<std::string, std::string>&
ProcessSnapshotFuchsia::AnnotationsSimpleMap() const {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
  return annotations_simple_map_;
}

const SystemSnapshot* ProcessSnapshotFuchsia::System() const {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
  return nullptr;
}

std::vector<const ThreadSnapshot*> ProcessSnapshotFuchsia::Threads() const {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
  return std::vector<const ThreadSnapshot*>();
}

std::vector<const ModuleSnapshot*> ProcessSnapshotFuchsia::Modules() const {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
  return std::vector<const ModuleSnapshot*>();
}

std::vector<UnloadedModuleSnapshot> ProcessSnapshotFuchsia::UnloadedModules()
    const {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
  return std::vector<UnloadedModuleSnapshot>();
}

const ExceptionSnapshot* ProcessSnapshotFuchsia::Exception() const {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
  return nullptr;
}

std::vector<const MemoryMapRegionSnapshot*> ProcessSnapshotFuchsia::MemoryMap()
    const {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
  return std::vector<const MemoryMapRegionSnapshot*>();
}

std::vector<HandleSnapshot> ProcessSnapshotFuchsia::Handles() const {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
  return std::vector<HandleSnapshot>();
}

std::vector<const MemorySnapshot*> ProcessSnapshotFuchsia::ExtraMemory() const {
  NOTREACHED();  // TODO(scottmg): https://crashpad.chromium.org/bug/196
  return std::vector<const MemorySnapshot*>();
}

void ProcessSnapshotFuchsia::InitializeThreads() {
  size_t num_threads;
  zx_status_t status = zx_object_get_info(
      process_, ZX_INFO_PROCESS_THREADS, nullptr, 0, nullptr, &num_threads);
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "zx_object_get_info ZX_INFO_PROCESS_THREADS";
    return;
  }

  auto threads = std::make_unique<zx_koid_t[]>(num_threads);
  size_t records_read;
  status = zx_object_get_info(process_,
                              ZX_INFO_PROCESS_THREADS,
                              threads.get(),
                              num_threads * sizeof(threads[0]),
                              &records_read,
                              nullptr);
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "zx_object_get_info ZX_INFO_PROCESS_THREADS";
    return;
  }
}

void ProcessSnapshotFuchsia::InitializeModules() {
  char name[ZX_MAX_NAME_LEN];
  zx_status_t status =
      zx_object_get_property(process_, ZX_PROP_NAME, name, sizeof(name));
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "zx_object_get_property";
    strlcpy(name, "app", sizeof(name));
  }

  uintptr_t lmap, debug_addr;
  status = zx_object_get_property(
      process_, ZX_PROP_PROCESS_DEBUG_ADDR, &debug_addr, sizeof(debug_addr));
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status)
        << "zx_object_get_property ZX_PROP_PROCESS_DEBUG_ADDR";
    return;
  }

  constexpr auto kOffset_r_debug_map = offsetof(struct r_debug, r_map);

  status = ReadMemory(
      process_, debug_addr + kOffset_r_debug_map, &lmap, sizeof(lmap));
  if (status != ZX_OK) {
    ZX_LOG(ERROR, status) << "zx_process_read_memory lmap";
    return;
  }

  constexpr auto kOffset_link_map_next = offsetof(struct link_map, l_next);
  constexpr auto kOffset_link_map_name = offsetof(struct link_map, l_name);
  constexpr auto kOffset_link_map_addr = offsetof(struct link_map, l_addr);

  int iter = 0;
  while (lmap != 0) {
    if (iter++ > 500) {
      LOG(ERROR) << "InitializeModules() too many entries, aborting";
      break;
    }

    zx_vaddr_t base;
    status =
        ReadMemory(process_, lmap + kOffset_link_map_addr, &base, sizeof(base));
    if (status != ZX_OK) {
      ZX_LOG(ERROR, status) << "zx_process_read_memory base";
      break;
    }

    uintptr_t next;
    status =
        ReadMemory(process_, lmap + kOffset_link_map_next, &next, sizeof(next));
    if (status != ZX_OK) {
      ZX_LOG(ERROR, status) << "zx_process_read_memory next";
      break;
    }

    uintptr_t str;
    status =
        ReadMemory(process_, lmap + kOffset_link_map_name, &str, sizeof(str));
    if (status != ZX_OK) {
      ZX_LOG(ERROR, status) << "zx_process_read_memory str";
      break;
    }

    char dsoname[64];
    status = ReadString(process_, str, dsoname, sizeof(dsoname));
    if (status != ZX_OK) {
      ZX_LOG(ERROR, status) << "ReadString";
      break;
    }

    LOG(ERROR) << "DSONAME=" << (dsoname[0] ? dsoname : name);

// From zircon/third_party/ulib/musl/ldso/dynlink.c.
#define MAX_BUILDID_SIZE 64
    char buildid[MAX_BUILDID_SIZE * 2 + 1];

    ReadBuildID(process_, base, buildid, sizeof(buildid));

    auto module = std::make_unique<internal::ModuleSnapshotFuchsia>();
    module->Initialize(dsoname[0] ? dsoname : name, base, 0, buildid);
    modules_.push_back(std::move(module));

    lmap = next;
  }
}

}  // namespace crashpad
