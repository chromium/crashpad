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

#include "util/linux/process_memory.h"

#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/wait.h>

#include "base/logging.h"
#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#include "util/linux/scoped_ptrace_attach.h"

namespace crashpad {

ProcessMemory::ProcessMemory(pid_t pid) : pid_(pid) {}

ProcessMemory::~ProcessMemory() {}

bool ProcessMemory::Read(LinuxVMAddress address,
                         LinuxVMSize size,
                         void* buffer) const {
  return ProcFSRead(address, size, buffer);
}

bool ProcessMemory::ReadCString(LinuxVMAddress address,
                                std::string* string) const {
  return ReadCStringInternal(address, false, 0, string);
}

bool ProcessMemory::ReadCStringSizeLimited(LinuxVMAddress address,
                                           LinuxVMSize size,
                                           std::string* string) const {
  return ReadCStringInternal(address, true, size, string);
}

bool ProcessMemory::ReadCStringInternal(LinuxVMAddress address,
                                        bool has_size,
                                        LinuxVMSize size,
                                        std::string* string) const {
  string->clear();

  char path[32];
  snprintf(path, sizeof(path), "/proc/%d/mem", pid_);
  base::ScopedFD fd(HANDLE_EINTR(open(path, O_RDONLY)));
  if (fd.get() < 0) {
    PLOG(ERROR) << "open";
    return false;
  }

  char buffer[4096];
  ssize_t bytes;
  do {
    size_t read_size;
    if (has_size) {
      read_size = std::min(sizeof(buffer), size);
    } else {
      read_size = sizeof(buffer);
    }
    bytes = HANDLE_EINTR(pread(fd.get(), buffer, read_size, address));
    if (bytes < 0) {
      PLOG(ERROR) << "pread";
      return false;
    }
    if (bytes == 0) {
      break;
    }

    char* nul = static_cast<char*>(memchr(buffer, '\0', bytes));
    if (nul != nullptr) {
      string->append(buffer, nul - buffer);
      return true;
    }
    string->append(buffer, bytes);

    address += bytes;
    size -= bytes;
  } while (!has_size || size > 0);

  LOG(ERROR) << "missing nul-terminator";
  return false;
}

bool ProcessMemory::ProcFSRead(LinuxVMAddress address,
                               LinuxVMSize size,
                               void* buffer) const {
  char path[32];
  snprintf(path, sizeof(path), "/proc/%d/mem", pid_);
  base::ScopedFD fd(HANDLE_EINTR(open(path, O_RDONLY)));
  if (fd.get() < 0) {
    PLOG(ERROR) << "open";
    return false;
  }

  char* buffer_c = static_cast<char*>(buffer);
  while (size > 0) {
    ssize_t bytes = HANDLE_EINTR(pread(fd.get(), buffer_c, size, address));
    if (bytes < 0) {
      PLOG(ERROR) << "pread";
      return false;
    }
    size -= bytes;
    address += bytes;
    buffer_c += bytes;
  }
  return true;
}

bool ProcessMemory::PtraceRead(LinuxVMAddress address,
                               LinuxVMSize size,
                               void* buffer) const {
  char* buffer_c = static_cast<char*>(buffer);

  ScopedPtraceAttach ptrace_attach(pid_);
  if (!ptrace_attach.Attached()) {
    return false;
  }

  for (LinuxVMSize offset = 0; offset < size; offset += sizeof(long)) {
    errno = 0;
    long word =
        ptrace(PTRACE_PEEKDATA, pid_, address + offset, buffer_c + offset);
    if (errno != 0) {
      PLOG(ERROR) << "ptrace";
      return false;
    }
    memcpy(buffer_c + offset,
           &word,
           std::min(size - offset, static_cast<LinuxVMSize>(sizeof(word))));
  }

  return true;
}

}  // namespace crashpad
