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

#include "snapshot/linux/process_reader.h"

#include <dirent.h>
#include <sched.h>
#include <sys/resource.h>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "util/linux/memory_map.h"
#include "util/posix/scoped_dir.h"

namespace crashpad {

namespace {

bool ShouldMergeStackMappings(const MemoryMap::Mapping* stack_mapping,
                              const MemoryMap::Mapping* adj_mapping) {
  return adj_mapping->readable &&
         stack_mapping->device == adj_mapping->device &&
         stack_mapping->inode == adj_mapping->inode &&
         stack_mapping->offset == adj_mapping->offset &&
         (adj_mapping->name == "[stack]" || adj_mapping->name == "");
}

}  // namespace

ProcessReader::Thread::Thread() {}

ProcessReader::Thread::~Thread() {}

ProcessReader::Module::Module() {}

ProcessReader::Module::~Module() {}

ProcessReader::ProcessReader()
    : process_info_(),
      memory_map_(),
      threads_(),
      process_memory_(),
      initialized_(),
      is_64_bit_(false),
      initialized_threads_(false) {}

ProcessReader::~ProcessReader() {}

bool ProcessReader::Initialize(pid_t pid) {
  if (!process_info_.Initialize(pid)) {
    return false;
  }

  if (!memory_map_.Initialize(pid)) {
    return false;
  }

  process_memory_.reset(new ProcessMemory());
  if (!process_memory_->Initialize(pid)) {
    return false;
  }

  if (!process_info_.Is64Bit(&is_64_bit_)) {
    return false;
  }

  return true;
}

const std::vector<ProcessReader::Thread>& ProcessReader::Threads() {
  if (!initialized_threads_) {
    InitializeThreads();
  }
  return threads_;
}

void ProcessReader::InitializeThreads() {
  DCHECK(threads_.empty());

  // Thread IDs
  {
    char path[32];
    snprintf(path, sizeof(path), "/proc/%d/task", ProcessID());
    DIR* dir = opendir(path);
    if (!dir) {
      PLOG(ERROR) << "opendir";
      return;
    }
    ScopedDIR scoped_dir(dir);

    dirent* dir_entry;
    while ((dir_entry = readdir(scoped_dir.get()))) {
      if (strncmp(dir_entry->d_name, ".", sizeof(dir_entry->d_name)) == 0 ||
          strncmp(dir_entry->d_name, "..", sizeof(dir_entry->d_name)) == 0) {
        continue;
      }
      Thread thread;
      if (!base::StringToInt(dir_entry->d_name, &thread.tid)) {
        LOG(ERROR) << "format error";
        continue;
      }

      if (threads_.size() > 0 && thread.tid == ProcessID()) {
        threads_.push_back(threads_[0]);
        threads_[0] = thread;
      } else {
        threads_.push_back(thread);
      }
    }
  }

  if (ProcessID() == getpid()) {
    // TODO(jperaza): ptrace can't be used on threads in the same thread group.
    // Using clone to create a new thread in it's own thread group doesn't work
    // because libc doesn't support threads it didn't create via pthreads.
    // It's probably also not a great idea to be constructing a snapshot in the
    // same address space we are tracing.
    // Fork a new process to snapshot us and copy the data back?
    LOG(ERROR) << "not implemented";
  } else {
    return PtraceInitializeThreads();
  }
}

bool ProcessReader::PtraceInitializeThreads() {
  for (Thread& thread : threads_) {
    thread.initialized.set_invalid();
    ThreadInfo thread_info;
    if (!thread_info.Initialize(thread.tid)) {
      continue;
    }

    thread_info.GetGeneralPurposeRegisters(&thread.thread_context);

    if (!thread_info.GetFloatingPointRegisters(&thread.float_context)) {
      continue;
    }

    if (!thread_info.GetThreadArea(&thread.thread_specific_data_address)) {
      continue;
    }

    // TODO(jperaza): Is there any better value for this? Threads are
    // necessarily suspended for ptracing.
    thread.suspend_count = 1;

    // TODO(jperaza): Starting with Linux 3.14, scheduling policy, static
    // priority, and nice value can be collected with sched_getattr().
    int res = sched_getscheduler(thread.tid);
    if (res < 0) {
      PLOG(ERROR) << "sched_getscheduler";
      continue;
    }
    thread.sched_policy = res;

    sched_param param;
    if (sched_getparam(thread.tid, &param) != 0) {
      PLOG(ERROR) << "sched_getparam";
      continue;
    }
    thread.static_priority = param.sched_priority;

    errno = 0;
    res = getpriority(PRIO_PROCESS, thread.tid);
    if (res == -1 && errno) {
      PLOG(ERROR) << "getpriority";
      continue;
    }
    thread.nice_value = res;


    // Stack memory
    LinuxVMAddress stack_pointer;
#if defined(ARCH_CPU_X86_FAMILY)
    stack_pointer = is_64_bit_ ? thread.thread_context.t64.rsp
                               : thread.thread_context.t32.esp;
#elif defined(ARCH_CPU_ARM_FAMILY)
    stack_pointer = is_64_bit_ ? thread.thread_context.t64.sp
                               : thread.thread_context.t32.sp;
#else
#error Port.
#endif

    // If we can't find the mapping, it's probably a bad stack pointer
    const MemoryMap::Mapping* mapping = memory_map_.FindMapping(stack_pointer);
    if (!mapping) {
      continue;
    }
    LinuxVMAddress stack_region_start = stack_pointer;

    // We've hit what looks like a guard page; skip to the end and check for a
    // mapped stack region.
    if (!mapping->readable) {
      stack_region_start = mapping->range.End();
      mapping = memory_map_.FindMapping(stack_region_start);
      if (!mapping) {
        continue;
      }
    } else {
#if defined(ARCH_CPU_X86_FAMILY)
      // Adjust start address to include the red zone
      if (Is64Bit()) {
        const LinuxVMSize kRedZoneSize = 128;
        LinuxVMAddress red_zone_base = stack_region_start >= kRedZoneSize
                                           ? stack_region_start - kRedZoneSize
                                           : 0;

        // Only include the red zone if it is part of a valid mapping
        if (red_zone_base >= mapping->range.Base()) {
          stack_region_start = red_zone_base;
        } else {
          const MemoryMap::Mapping* rz_mapping =
              memory_map_.FindMapping(red_zone_base);
          if (rz_mapping && ShouldMergeStackMappings(mapping, rz_mapping)) {
            stack_region_start = red_zone_base;
          } else {
            stack_region_start = mapping->range.Base();
          }
        }
      }
#endif
    }
    thread.stack_region_address = stack_region_start;

    // If there are more mappings at the end of this one, they may be a
    // continuation of the stack.
    LinuxVMAddress stack_end = mapping->range.End();
    const MemoryMap::Mapping* next_mapping;
    while ((next_mapping = memory_map_.FindMapping(stack_end)) &&
           ShouldMergeStackMappings(mapping, next_mapping)) {
      stack_end = next_mapping->range.End();
    }

    // The main thread should have an entry in the maps file just for its stack,
    // so we'll assume the base of the stack is at the end of the region. Other
    // threads' stacks may not have their own entries in the maps file if they
    // were user-allocated within a larger mapping, but pthreads places the TLS
    // at the high-address end of the stack so we can try using that to shrink
    // the stack region.
    thread.stack_region_size = stack_end - thread.stack_region_address;
    if (thread.tid != ProcessID() &&
        thread.thread_specific_data_address > thread.stack_region_address &&
        thread.thread_specific_data_address < stack_end) {
      thread.stack_region_size =
          thread.thread_specific_data_address - thread.stack_region_address;
    }
    thread.initialized.set_valid();
  }
  return true;
}

void ProcessReader::InitializeModules() {
  AuxiliaryVector auxv;
  if (!auxv.Initialize(ProcessID(), is_64_bit_) {
    return;
  }
  LinuxVMAddress phdr_base;
  if (!auxv.GetValue(AT_PHDR, &phdr_base)) {
    LOG(ERROR) << "no phdrs";
    return;
  }

  const MemoryMap::Mapping* exe_mapping = memory_map_.FindMapping(phdr_base);
  if (!exe_mapping) {
    LOG(ERROR) << "phdrs aren't mapped";
    return;
  }
  LinuxVMAddress elf_address = exe_mapping->range.Base();

  std::unique_ptr<ElfImageReader> exe_reader(new ElfImageReader());
  if (!exe_reader->Initialize(process_memory_.get(),
                              elf_address)) {
    return;
  }
  modules_.push_back(exe_reader.release());

  LinuxVMAddress debug_address;
  if (!exe_reader->GetDebug(&debug_address)) {
    LOG(ERROR) << "no debug address";
    return;
  }

  DebugRendezvous debug;
  if (!debug.Initialize(debug_address, is_64_bit_)) {
    return;
  }

  for (const DebugRendezvous::LinkEntry& link_entry : debug.Modules()) {
    Module module;
    // TODO(jperaza): Use the link entry name or the mapping name?
    module.name = link_entry.name();
    modules_.push_back(module);

    MemoryMap::Mapping* dynamic_mapping =
        FindMapping(link_entry.dynamic_section);
    if (!dynamic_mapping) {
      continue;
    }
    MemoryMap::Mapping* module_mapping =
        FindMappingWithName(dynamic_mapping->name);
    if (!module_mapping) {
      continue;
    }
    std::unique_ptr<ElfImageReader> module_reader(new ElfImageReader());
    if (!module_reader->Initialize(process_memory_.get(),
                                   module_mapping.range.Base())) {
      continue;
    }
    module.reader = module_reader.get();
    module_readers_.push_back(module_reader.release());
  }
}

}  // namespace crashpad
