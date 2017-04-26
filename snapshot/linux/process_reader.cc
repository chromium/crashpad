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

#include <asm/prctl.h>
#include <dirent.h>
#include <elf.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/uio.h>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "util/linux/memory_map.h"
#include "util/posix/scoped_dir.h"

#include <memory>

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

  // POSIX specifies that priorities are per-process. However, the current
  // Linux implementation of pthreads allows priorities per-thread.
  for (auto& thread : threads_) {
    errno = 0;
    int res = getpriority(PRIO_PROCESS, thread.tid);
    if (res == -1 && errno) {
      PLOG(ERROR) << "getpriority";
    }
    thread.priority = res;
  }

  // TODO(jperaza): Set suspend count. Can this info be collected on linux?
  // Ptrace will be modifying the run state of the target process; should that
  // matter?

  if (ProcessID() == getpid()) {
    SelfInitializeThreads();
  } else {
    PtraceInitializeThreads();
  }
}

void ProcessReader::SelfInitializeThreads() {
  // TODO(jperaza): Collect information from sibling threads. Ptrace can not be
  // used on threads in the same thread group. We can spawn a new thread in a
  // different thread group to collect the information.
}

void ProcessReader::PtraceInitializeThreads() {
  for (Thread& thread : threads_) {
    ScopedPtraceAttach ptrace_attach;
    if (!ptrace_attach.ResetAttach(thread.tid)) {
      continue;
    }

    // General purpose registers
    iovec iov;
    iov.iov_base = &thread.thread_context;
    iov.iov_len = sizeof(thread.thread_context);
    if (ptrace(PTRACE_GETREGSET,
               thread.tid,
               reinterpret_cast<void*>(NT_PRSTATUS),
               &iov) != 0) {
      PLOG(ERROR) << "ptrace";
      continue;
    }

    // Floating point registers
    iov.iov_base = &thread.float_context;
    iov.iov_len = sizeof(thread.float_context);
    if (ptrace(PTRACE_GETREGSET,
               thread.tid,
               reinterpret_cast<void*>(NT_PRFPREG),
               &iov) != 0) {
      PLOG(ERROR) << "ptrace";
      continue;
    }

// TLS
#if defined(ARCH_CPU_ARM64) || defined(ARCH_CPU_ARMEL)
    iov.iov_base = &thread.thread_specific_data_address;
    iov.iov_len = sizeof(thread.thread_specific_data_address);
    if (ptrace(PTRACE_GETREGSET,
               thread.tid,
               reinterpret_cast<void*>(NT_ARM_TLS),
               &iov) != 0) {
      PLOG(ERROR) << "ptrace";
      continue;
    }
    if (iov.iov_len != sizeof(thread.thread_specific_data_address)) {
      LOG(ERROR) << "thread address size mismatch";
    }
#elif defined(ARCH_CPU_X86_FAMILY)
    if (Is64Bit()) {
      thread.thread_specific_data_address = thread.thread_context.fs_base;
    } else {
      thread.thread_specific_data_address = thread.thread_context.gs;
    }
#endif

    // Stack memory
    LinuxVMAddress stack_pointer = STACK_POINTER(thread.thread_context);

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

    if (thread.tid == ProcessID()) {
      // The main thread should have an entry in the maps file just for its
      // stack, so we'll assume the base of the stack is at the end of the
      // region. Also try to merge neighboring regions if they look like stack.
      LinuxVMAddress stack_end = mapping->range.End();
      const MemoryMap::Mapping* next_mapping;
      while ((next_mapping = memory_map_.FindMapping(stack_end)) &&
             ShouldMergeStackMappings(mapping, next_mapping)) {
        stack_end = next_mapping->range.End();
      }
      thread.stack_region_size = stack_end - thread.stack_region_address;
    } else {
      // Other threads' stacks may not have their own entries in the maps file
      // but pthreads places the TLS at the high-address end of the stack.
      thread.stack_region_size =
          thread.thread_specific_data_address - thread.stack_region_address;
    }
  }
}

}  // namespace crashpad
