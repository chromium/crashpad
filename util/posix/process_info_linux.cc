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
#include <linux/uio.h>
#include <stdio.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/files/scoped_file.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_number_conversions.h"
#include "util/file/file_reader.h"

namespace crashpad {

namespace {

// If the string |pattern| is matched exactly at the start of |input|, advance
// |input| past |pattern| and return true.
bool AdvancePastPrefix(const char** input, const char* pattern) {
  size_t length = strlen(pattern);
  if (strncmp(*input, pattern, length) == 0) {
    *input += length;
    return true;
  }
  return false;
}

#define make_adapter(type, function)                                 \
  bool StringToNumber(const base::StringPiece& input, type* value) { \
    return function(input, value);                                   \
  }
make_adapter(int, base::StringToInt)
make_adapter(unsigned int, base::StringToUint)
make_adapter(uint64_t, base::StringToUint64)
#undef make_adapter

// Attempt to convert a prefix of |input| to numeric type T. On success, set
// |value| to the number, advance |input| past the number, and return true.
template <typename T>
bool AdvancePastNumber(const char** input, T* value) {
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

bool AdvanceToWhitespace(const char** line_ptr) {
  while (**line_ptr != '\t' && **line_ptr != ' ' && **line_ptr != '\n') {
    if (**line_ptr == '\0') {
      return false;
    }
    ++(*line_ptr);
  }
  return true;
}

void AdvancePastNull(const char** line_ptr) {
  while (**line_ptr != '\0') {
    ++(*line_ptr);
  }
  ++(*line_ptr);
}

struct BufferFreer {
  void operator()(char** buffer_ptr) const {
    free(*buffer_ptr);
    *buffer_ptr = nullptr;
  }
};
typedef std::unique_ptr<char*, BufferFreer> ScopedBufferPtr;

void SubtractTimespec(const timespec& t1, const timespec& t2, timespec* t3) {
  t3->tv_sec = t1.tv_sec - t2.tv_sec;
  t3->tv_nsec = t1.tv_nsec - t2.tv_nsec;
  if (t3->tv_nsec < 0) {
    t3->tv_nsec += 1000000000;
    t3->tv_sec -= 1;
  }
}

}  // namespace

ProcessInfo::ProcessInfo()
    : supplementary_groups_(),
      start_time_(),
      pid_(-1),
      ppid_(-1),
      uid_(-1),
      euid_(-1),
      suid_(-1),
      gid_(-1),
      egid_(-1),
      sgid_(-1),
      is_64_bit_(false),
      initialized_() {}

ProcessInfo::~ProcessInfo() {}

bool ProcessInfo::Initialize(pid_t pid) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  pid_ = pid;

  size_t buffer_size = 0;
  char* buffer = nullptr;
  ScopedBufferPtr scoper(&buffer);

  {
    char path[32];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    base::ScopedFILE status_file(fopen(path, "re"));
    if (!status_file.get()) {
      PLOG(ERROR) << "fopen" << path;
      return false;
    }

    bool have_ppid = false;
    bool have_uids = false;
    bool have_gids = false;
    bool have_groups = false;
    ssize_t len;
    while ((len = getline(&buffer, &buffer_size, status_file.get())) > 0) {
      const char* line = buffer;

      if (AdvancePastPrefix(&line, "PPid:\t")) {
        if (have_ppid) {
          LOG(ERROR) << "format error: multiple PPid lines";
          return false;
        }
        have_ppid = AdvancePastNumber(&line, &ppid_);
        if (!have_ppid) {
          LOG(ERROR) << "format error: unrecognized PPid format";
          return false;
        }
      } else if (AdvancePastPrefix(&line, "Uid:\t")) {
        if (have_uids) {
          LOG(ERROR) << "format error: multiple Uid lines";
          return false;
        }
        have_uids =
            AdvancePastNumber(&line, &uid_) &&
            AdvancePastPrefix(&line, "\t") &&
            AdvancePastNumber(&line, &euid_) &&
            AdvancePastPrefix(&line, "\t") &&
            AdvancePastNumber(&line, &suid_);
        if (!have_uids) {
          LOG(ERROR) << "format error: unrecognized Uid format";
          return false;
        }
      } else if (AdvancePastPrefix(&line, "Gid:\t")) {
        if (have_gids) {
          LOG(ERROR) << "format error: multiple Gid lines";
          return false;
        }
        have_gids =
            AdvancePastNumber(&line, &gid_) &&
            AdvancePastPrefix(&line, "\t") &&
            AdvancePastNumber(&line, &egid_) &&
            AdvancePastPrefix(&line, "\t") &&
            AdvancePastNumber(&line, &sgid_);
        if (!have_gids) {
          LOG(ERROR) << "format error: unrecognized Gid format";
          return false;
        }
      } else if (AdvancePastPrefix(&line, "Groups:\t")) {
        if (have_groups) {
          LOG(ERROR) << "format error: multiple Groups lines";
          return false;
        }
        gid_t group;
        while (AdvancePastNumber(&line, &group)) {
          supplementary_groups_.insert(group);
          if (!AdvancePastPrefix(&line, " ")) {
            LOG(ERROR) << "format error";
            return false;
          }
        }
        if (!AdvancePastPrefix(&line, "\n") || line != buffer + len) {
          LOG(ERROR) << "format error: unrecognized Groups format";
          return false;
        }
        have_groups = true;
      }
    }
    if (!feof(status_file.get())) {
      PLOG(ERROR) << "getline";
      return false;
    }
    if (!have_ppid || !have_uids || !have_gids || !have_groups) {
      LOG(ERROR) << "format error: missing fields";
      return false;
    }
  }

  {
    char path[32];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    base::ScopedFILE stat_file(fopen(path, "re"));
    if (!stat_file.get()) {
      PLOG(ERROR) << "fopen" << path;
      return false;
    }
    ssize_t length = getline(&buffer, &buffer_size, stat_file.get());
    if (length < 1) {
      if (!feof(stat_file.get())) {
        PLOG(ERROR) << "getline";
      } else {
        LOG(ERROR) << "format error: premature EOF";
      }
      return false;
    }

    // The process start time is the 22nd column in the line.
    // The second column is the executable name in parentheses.
    // The executable name may have parentheses itself, so find the third
    // column by working backwards to find the last closing parens and then
    // count forward to the 22nd column.
    const char* line_ptr = buffer + length;
    while (--line_ptr > buffer && *line_ptr != ')') {
    }
    if (*line_ptr != ')') {
      LOG(ERROR) << "format error";
      return false;
    }

    for (int index = 2; index < 22 && line_ptr < buffer + length; ++index) {
      ++line_ptr;
      if (!AdvanceToWhitespace(&line_ptr)) {
        LOG(ERROR) << "format error";
        return false;
      }
    }
    if (*line_ptr != ' ') {
      LOG(ERROR) << "format error";
      return false;
    }
    ++line_ptr;

    // start time is in jiffies instead of clock ticks pre 2.6.
    uint64_t ticks_after_boot;
    if (!AdvancePastNumber<uint64_t>(&line_ptr, &ticks_after_boot)) {
      LOG(ERROR) << "format error";
      return false;
    }
    long clock_ticks_per_s = sysconf(_SC_CLK_TCK);
    if (clock_ticks_per_s <= 0) {
      PLOG(ERROR) << "sysconf";
      return false;
    }
    time_t seconds_after_boot = ticks_after_boot / clock_ticks_per_s;

    timespec uptime;
    if (clock_gettime(CLOCK_BOOTTIME, &uptime) < 0) {
      PLOG(ERROR) << "clock_gettime";
      return false;
    }

    timespec current_time;
    if (clock_gettime(CLOCK_REALTIME, &current_time) == -1) {
      PLOG(ERROR) << "clock_gettime";
      return false;
    }

    timespec boot_time;
    SubtractTimespec(current_time, uptime, &boot_time);

    start_time_.tv_usec = boot_time.tv_nsec / 1000;
    start_time_.tv_sec = boot_time.tv_sec + seconds_after_boot;
  }

#if defined(ARCH_CPU_64_BITS)
  const bool am_64_bit = true;
#else
  const bool am_64_bit = false;
#endif

  pid_t mypid = getpid();
  if (pid == mypid) {
    is_64_bit_ = am_64_bit;
  } else {
    if (ptrace(PTRACE_ATTACH, pid_, nullptr, nullptr) != 0) {
      PLOG(ERROR) << "ptrace";
      return false;
    }

    if (HANDLE_EINTR(waitpid(pid_, nullptr, __WALL)) < 0) {
      PLOG(ERROR) << "waitpid";
      return false;
    }

    // Allocate more buffer space than is required to hold registers for this
    // process. If the kernel fills the extra space, the target process uses
    // more/larger registers than this process. If the kernel fills less space
    // than sizeof(regs) then the target process uses smaller/fewer registers.
    struct {
      user_pt_regs regs;
      char extra;
    } regbuf;

    iovec iovec;
    iovec.iov_base = &regbuf;
    iovec.iov_len = sizeof(regbuf);
    if (ptrace(PTRACE_GETREGSET,
               pid_,
               reinterpret_cast<void*>(NT_PRSTATUS),
               &iovec) != 0) {
      PLOG(ERROR) << "ptrace";
      return false;
    }

    is_64_bit_ = am_64_bit ? iovec.iov_len == sizeof(regbuf.regs)
                           : iovec.iov_len != sizeof(regbuf.regs);

    if (ptrace(PTRACE_DETACH, pid_, nullptr, nullptr) != 0) {
      PLOG(ERROR) << "ptrace";
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

  size_t buffer_size = 0;
  char* buffer = nullptr;
  ScopedBufferPtr scoper(&buffer);

  std::vector<std::string> local_argv;
  {
    char path[32];
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid_);
    base::ScopedFILE cmd_file(fopen(path, "re"));
    if (!cmd_file.get()) {
      PLOG(ERROR) << "fopen" << path;
      return false;
    }
    ssize_t length = getline(&buffer, &buffer_size, cmd_file.get());
    if (length < 0) {
      PLOG(ERROR) << "getline";
      return false;
    }
    if (!feof(cmd_file.get())) {
      LOG(ERROR) << "format error";
      return false;
    }

    for (const char* line_ptr = buffer; line_ptr < buffer + length;
         AdvancePastNull(&line_ptr)) {
      local_argv.push_back(line_ptr);
    }
  }
  argv->swap(local_argv);
  return true;
}

}  // namespace crashpad
