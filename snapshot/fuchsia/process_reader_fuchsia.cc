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

#include "snapshot/fuchsia/process_reader_fuchsia.h"

#include <link.h>
#include <zircon/syscalls.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/scoped_zx_handle.h"
#include "base/logging.h"

namespace crashpad {

ProcessReaderFuchsia::Module::Module() = default;

ProcessReaderFuchsia::Module::~Module() = default;

ProcessReaderFuchsia::Thread::Thread() = default;

ProcessReaderFuchsia::Thread::~Thread() = default;

ProcessReaderFuchsia::ProcessReaderFuchsia() = default;

ProcessReaderFuchsia::~ProcessReaderFuchsia() = default;

bool ProcessReaderFuchsia::Initialize(zx_handle_t process) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  process_ = process;

  process_memory_.reset(new ProcessMemoryFuchsia());
  process_memory_->Initialize(process_);

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

const std::vector<ProcessReaderFuchsia::Module>&
ProcessReaderFuchsia::Modules() {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  if (!initialized_modules_) {
    InitializeModules();
  }

  return modules_;
}

const std::vector<ProcessReaderFuchsia::Thread>&
ProcessReaderFuchsia::Threads() {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  if (!initialized_threads_) {
    InitializeThreads();
  }

  return threads_;
}

void ProcessReaderFuchsia::InitializeModules() {
  DCHECK(!initialized_modules_);
  DCHECK(modules_.empty());

  initialized_modules_ = true;

  // TODO(scottmg): <inspector/inspector.h> does some of this, but doesn't
  // expose any of the data that's necessary to fill out a Module after it
  // retrieves (some of) the data into internal structures. It may be worth
  // trying to refactor/upstream some of this into Fuchsia.

  std::string app_name("app:");
  {
    char name[ZX_MAX_NAME_LEN];
    zx_status_t status =
        zx_object_get_property(process_, ZX_PROP_NAME, name, sizeof(name));
    if (status != ZX_OK) {
      LOG(ERROR) << "zx_object_get_property ZX_PROP_NAME";
      return;
    }

    app_name += name;
  }

  // Starting from the ld.so's _dl_debug_addr, read the link_map structure and
  // walk the list to fill out modules_.

  uintptr_t debug_address;
  zx_status_t status = zx_object_get_property(process_,
                                              ZX_PROP_PROCESS_DEBUG_ADDR,
                                              &debug_address,
                                              sizeof(debug_address));
  if (status != ZX_OK || debug_address == 0) {
    LOG(ERROR) << "zx_object_get_property ZX_PROP_PROCESS_DEBUG_ADDR";
    return;
  }

  constexpr auto k_r_debug_map_offset = offsetof(r_debug, r_map);
  uintptr_t map;
  if (!process_memory_->Read(
          debug_address + k_r_debug_map_offset, sizeof(map), &map)) {
    LOG(ERROR) << "read link_map";
    return;
  }

  int i = 0;
  constexpr int kMaxDso = 1000;  // Stop after an unreasonably large number.
  while (map != 0) {
    if (++i >= kMaxDso) {
      LOG(ERROR) << "possibly circular dso list, terminating";
      return;
    }

    constexpr auto k_link_map_addr_offset = offsetof(link_map, l_addr);
    zx_vaddr_t base;
    if (!process_memory_->Read(
            map + k_link_map_addr_offset, sizeof(base), &base)) {
      LOG(ERROR) << "Read base";
      // Could theoretically continue here, but realistically if any part of
      // link_map fails to read, things are looking bad, so just abort.
      break;
    }

    constexpr auto k_link_map_next_offset = offsetof(link_map, l_next);
    zx_vaddr_t next;
    if (!process_memory_->Read(
            map + k_link_map_next_offset, sizeof(next), &next)) {
      LOG(ERROR) << "Read next";
      break;
    }

    constexpr auto k_link_map_name_offset = offsetof(link_map, l_name);
    zx_vaddr_t name_address;
    if (!process_memory_->Read(map + k_link_map_name_offset,
                               sizeof(name_address),
                               &name_address)) {
      LOG(ERROR) << "Read name address";
      break;
    }

    std::string dsoname;
    if (!process_memory_->ReadCString(name_address, &dsoname)) {
      // In this case, it could be reasonable to continue on to the next module
      // as this data isn't strictly in the link_map.
      LOG(ERROR) << "ReadCString name";
    }

    Module module;
    if (dsoname.empty()) {
      module.name = app_name;
      module.type = ModuleSnapshot::kModuleTypeExecutable;
    } else {
      module.name = dsoname;
      // TODO(scottmg): Handle kModuleTypeDynamicLoader.
      module.type = ModuleSnapshot::kModuleTypeSharedLibrary;
    }

    std::unique_ptr<ElfImageReader> reader(new ElfImageReader());

    std::unique_ptr<ProcessMemoryRange> process_memory_range(
        new ProcessMemoryRange());
    // TODO(scottmg): Could this be limited range?
    process_memory_range->Initialize(process_memory_.get(), true);
    process_memory_ranges_.push_back(std::move(process_memory_range));

    reader->Initialize(*process_memory_ranges_.back(), base);
    module.reader = reader.get();
    module_readers_.push_back(std::move(reader));
    modules_.push_back(module);

    map = next;
  }
}

void ProcessReaderFuchsia::InitializeThreads() {
  DCHECK(!initialized_threads_);
  DCHECK(threads_.empty());

  initialized_threads_ = true;

  // Retrieve the thread koids. This is racy; better if the process is suspended
  // itself, but threads could still be externally created. As there's no
  // maximum, this needs to be retried in a loop until the actual threads
  // retrieved is equal to the available threads.

  std::vector<zx_koid_t> threads(100);
  size_t actual_num_threads, available_num_threads;
  for (;;) {
    zx_status_t status = zx_object_get_info(process_,
                                            ZX_INFO_PROCESS_THREADS,
                                            &threads[0],
                                            sizeof(threads[0]) * threads.size(),
                                            &actual_num_threads,
                                            &available_num_threads);
    // If the buffer is too small (even zero), the result is still ZX_OK, not
    // ZX_ERR_BUFFER_TOO_SMALL.
    if (status != ZX_OK) {
      ZX_LOG(ERROR, status) << "zx_object_get_info ZX_INFO_PROCESS_THREADS";
      break;
    }
    if (actual_num_threads == available_num_threads) {
      threads.resize(actual_num_threads);
      break;
    }

    // Resize to the expected number next time with a bit extra to attempt to
    // handle the race between here and the next request.
    threads.resize(available_num_threads + 10);
  }

  for (const zx_koid_t thread_koid : threads) {
    zx_handle_t raw_handle;
    zx_status_t status = zx_object_get_child(
        process_, thread_koid, ZX_RIGHT_SAME_RIGHTS, &raw_handle);
    if (status != ZX_OK) {
      ZX_LOG(ERROR, status) << "zx_object_get_child";
      // TODO(scottmg): Decide if it's worthwhile adding a mostly-empty Thread
      // here, consisting only of the koid, but no other information. The only
      // time this is expected to happen is when there's a race between getting
      // the koid above, and requesting the handle here.
      continue;
    }

    base::ScopedZxHandle thread_handle(raw_handle);

    Thread thread;
    thread.id = thread_koid;

    char name[ZX_MAX_NAME_LEN] = {0};
    status = zx_object_get_property(
        thread_handle.get(), ZX_PROP_NAME, &name, sizeof(name));
    if (status != ZX_OK) {
      ZX_LOG(WARNING, status) << "zx_object_get_property ZX_PROP_NAME";
    } else {
      thread.name.assign(name);
    }

    zx_info_thread_t thread_info;
    status = zx_object_get_info(thread_handle.get(),
                                ZX_INFO_THREAD,
                                &thread_info,
                                sizeof(thread_info),
                                nullptr,
                                nullptr);
    if (status != ZX_OK) {
      ZX_LOG(WARNING, status) << "zx_object_get_info ZX_INFO_THREAD";
    } else {
      thread.state = thread_info.state;
    }

    threads_.push_back(thread);
  }
}

}  // namespace crashpad
