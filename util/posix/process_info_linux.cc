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

#include "util/posix/process_info.h"

#include <elf.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/files/scoped_file.h"
#include "third_party/lss/linux_syscall_support.h"
#include "util/file/file_reader.h"
#include "util/stdlib/string_number_conversion.h"

namespace crashpad {

namespace {
// If pattern is matched at the start of input, advance input past pattern and
// return true.
bool Chomp(char** input, const char* pattern) {
  size_t length = strlen(pattern);
  if (strncmp(*input, pattern, length) == 0) {
    *input += length;
    return true;
  }
  return false;
}

// Attempt to convert a prefix of input to numeric type T. On success, return
// true, set value to the number, and advance input past the number.
template <typename T>
bool ChompNumber(char** input, T* value) {
  size_t length = 0;
  if (std::numeric_limits<T>::is_signed && **input == '-') {
    ++length;
  }
  while (isdigit((*input)[length])) {
    ++length;
  }
  bool success = StringToNumber(base::StringPiece(*input, length), value);
  if (success) {
    *input += length;
    return true;
  }
  return false;
}

void AdvanceToWhitespace(char** line_ptr) {
  while (**line_ptr != '\t' && **line_ptr != ' ' && **line_ptr != '\n') {
    ++(*line_ptr);
  }
}

void AdvancePastNull(char** line_ptr) {
  while (**line_ptr != '\0') {
    ++(*line_ptr);
  }
  ++(*line_ptr);
}

struct BufferFreer {
  void operator()(char** buffer_ptr) const {
    if (buffer_ptr) {
      free(*buffer_ptr);
      *buffer_ptr = nullptr;
    }
  }
};
typedef std::unique_ptr<char*, BufferFreer> BufferScoper;
};  // namespace

ProcessInfo::ProcessInfo() : initialized_() {}

ProcessInfo::~ProcessInfo() {}

bool ProcessInfo::Initialize(pid_t pid) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  pid_ = pid;

  char path[32];

  size_t buffer_size = 0;
  char* buffer = nullptr;
  BufferScoper scoper(&buffer);

  snprintf(path, sizeof(path), "/proc/%d/status", pid);
  {
    base::ScopedFILE status_file(fopen(path, "re"));
    if (status_file.get() == nullptr) {
      LOG(ERROR) << "Unable to open file: " << path;
      return false;
    }

    bool have_ppid = false;
    bool have_uids = false;
    bool have_gids = false;
    bool have_groups = false;
    do {
      if (getline(&buffer, &buffer_size, status_file.get()) < 0) {
        LOG(ERROR) << "Couldn't read from " << path;
        return false;
      }

      char* line = buffer;
      if (!have_ppid && Chomp(&line, "PPid:\t")) {
        have_ppid = ChompNumber<pid_t>(&line, &ppid_);
      } else if (!have_uids && Chomp(&line, "Uid:\t")) {
        have_uids = ChompNumber<uid_t>(&line, &uid_) && Chomp(&line, "\t") &&
                    ChompNumber<uid_t>(&line, &euid_) && Chomp(&line, "\t") &&
                    ChompNumber<uid_t>(&line, &suid_);
      } else if (!have_gids && Chomp(&line, "Gid:\t")) {
        have_gids = ChompNumber<gid_t>(&line, &gid_) && Chomp(&line, "\t") &&
                    ChompNumber<gid_t>(&line, &egid_) && Chomp(&line, "\t") &&
                    ChompNumber<gid_t>(&line, &sgid_);
      } else if (!have_groups && Chomp(&line, "Groups:\t")) {
        have_groups = true;
        do {
          gid_t group;
          if (!ChompNumber<gid_t>(&line, &group)) {
            break;
          }
          supplementary_groups_.insert(group);
          Chomp(&line, " ");
        } while (true);
      }
    } while (!have_ppid || !have_uids || !have_gids || !have_groups);
  }

  snprintf(path, sizeof(path), "/proc/%d/stat", pid);
  {
    base::ScopedFILE stat_file(fopen(path, "re"));
    if (stat_file.get() == nullptr) {
      LOG(ERROR) << "Unable to open file " << path;
      return false;
    }
    ssize_t length = getline(&buffer, &buffer_size, stat_file.get());
    if (length < 1) {
      LOG(ERROR) << "Couldn't read from " << path;
      return false;
    }

    // The process start time is the 22nd column in the line.
    // The second column is the executable name in parentheses.
    // The executable name may have parentheses itself, so find the third
    // column by working backwards to find the last closing parens and then
    // count forward to the 22nd column.
    char* line_ptr = buffer + length;
    while (line_ptr > buffer + 1 && *(line_ptr - 1) != ')') {
      --line_ptr;
    }
    for (int index = 2; index < 22 && line_ptr < buffer + length; ++index) {
      ++line_ptr;
      AdvanceToWhitespace(&line_ptr);
    }
    ++line_ptr;

    // start time is in jiffies instead of clock ticks pre 2.6.
    uint64_t ticks_after_boot;
    if (!ChompNumber<uint64_t>(&line_ptr, &ticks_after_boot)) {
      LOG(ERROR) << "Couldn't read ticks after boot " << line_ptr;
      return false;
    }
    long clock_ticks_per_s = sysconf(_SC_CLK_TCK);
    if (clock_ticks_per_s <= 0) {
      LOG(ERROR) << "Couldn't get system clock tick rate";
      return false;
    }
    time_t seconds_after_boot = ticks_after_boot / clock_ticks_per_s;

    struct timespec uptime;
    if (clock_gettime(CLOCK_BOOTTIME, &uptime) == -1) {
      LOG(ERROR) << "Couldn't get boot time";
      return false;
    }

    struct timespec current_time;
    if (clock_gettime(CLOCK_REALTIME, &current_time) == -1) {
      LOG(ERROR) << "Couldn't get current time";
      return false;
    }

    start_time_.tv_usec = 0;
    start_time_.tv_sec =
        current_time.tv_sec - (uptime.tv_sec - seconds_after_boot);
  }

#if defined(ARCH_CPU_64_BITS)
    bool am_64_bit = true;
#else
    bool am_64_bit = false;
#endif

  pid_t mypid = sys_getpid();
  if (pid == mypid) {
    is_64_bit_ = am_64_bit;
  } else {
    if (sys_ptrace(PTRACE_ATTACH, pid_, nullptr, nullptr) != 0) {
      LOG(ERROR) << "Couldn't attach to process";
      return false;
    }

    while (sys_waitpid(pid_, nullptr, __WALL) < 0) {
      if (errno != EINTR) {
        LOG(ERROR) << "waitpid failed: " << errno;
        return false;
      }
    }

    struct {
      struct user_pt_regs regs;
      char extra;
    } regbuf;

    struct iovec iovec;
    iovec.iov_base = &regbuf;
    iovec.iov_len = sizeof(regbuf);
    if (sys_ptrace(PTRACE_GETREGSET,
                   pid_,
                   reinterpret_cast<void*>(NT_PRSTATUS),
                   &iovec) != 0) {
      LOG(ERROR) << "Couldn't get registers:" << errno;
      return false;
    }

    if (iovec.iov_len == sizeof(regbuf.regs)) {
      is_64_bit_ = am_64_bit;
    } else {
      is_64_bit_ = !am_64_bit;
    }

    if (sys_ptrace(PTRACE_DETACH, pid_, nullptr, nullptr) != 0) {
      LOG(ERROR) << "Couldn't detach from process";
    }
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

pid_t ProcessInfo::ProcessID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return pid_;
}

pid_t ProcessInfo::ParentProcessID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return ppid_;
}

uid_t ProcessInfo::RealUserID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return uid_;
}

uid_t ProcessInfo::EffectiveUserID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return euid_;
}

uid_t ProcessInfo::SavedUserID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return suid_;
}

gid_t ProcessInfo::RealGroupID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return gid_;
}

gid_t ProcessInfo::EffectiveGroupID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return egid_;
}

gid_t ProcessInfo::SavedGroupID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return sgid_;
}

std::set<gid_t> ProcessInfo::SupplementaryGroups() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return supplementary_groups_;
}

std::set<gid_t> ProcessInfo::AllGroups() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  std::set<gid_t> all_groups = SupplementaryGroups();
  all_groups.insert(RealGroupID());
  all_groups.insert(EffectiveGroupID());
  all_groups.insert(SavedGroupID());
  return all_groups;
}

bool ProcessInfo::DidChangePrivileges() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // TODO(jperaza) is this possible to determine?
  return false;
}

bool ProcessInfo::Is64Bit() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return is_64_bit_;
}

void ProcessInfo::StartTime(timeval* start_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *start_time = start_time_;
}

bool ProcessInfo::Arguments(std::vector<std::string>* argv) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  char path[32];

  size_t buffer_size = 0;
  char* buffer = nullptr;
  BufferScoper scoper(&buffer);

  std::vector<std::string> local_argv;
  snprintf(path, sizeof(path), "/proc/%d/cmdline", pid_);
  {
    base::ScopedFILE cmd_file(fopen(path, "re"));
    if (cmd_file.get() == nullptr) {
      LOG(ERROR) << "Unable to open file: " << path;
      return false;
    }
    ssize_t length = getline(&buffer, &buffer_size, cmd_file.get());
    if (length < 0) {
      LOG(ERROR) << "Couldn't read from " << path;
      return false;
    }

    for (char* line_ptr = buffer; line_ptr < buffer + length;
         AdvancePastNull(&line_ptr)) {
      local_argv.push_back(line_ptr);
    }
  }
  argv->swap(local_argv);
  return true;
}

}  // namespace crashpad
