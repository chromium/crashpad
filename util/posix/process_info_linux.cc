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

#include <ctype.h>
#include <elf.h>
#include <stdio.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/uio.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "util/file/file_reader.h"
#include "util/string/split_string.h"

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

#define MAKE_ADAPTER(type, function)                                        \
  bool ConvertStringToNumber(const base::StringPiece& input, type* value) { \
    return function(input, value);                                          \
  }
MAKE_ADAPTER(int, base::StringToInt)
MAKE_ADAPTER(unsigned int, base::StringToUint)
MAKE_ADAPTER(uint64_t, base::StringToUint64)
#undef MAKE_ADAPTER

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
  bool success = ConvertStringToNumber(base::StringPiece(*input, length),
                                       value);
  if (success) {
    *input += length;
    return true;
  }
  return false;
}

struct BufferFreer {
  void operator()(char** buffer_ptr) const {
    free(*buffer_ptr);
    *buffer_ptr = nullptr;
  }
};
using ScopedBufferPtr = std::unique_ptr<char*, BufferFreer>;

bool ReadEntireFile(const char* path, std::string* contents) {
  FileReader file;
  if (!file.Open(base::FilePath(path))) {
    return false;
  }

  char buffer[4096];
  FileOperationResult length;
  while ((length = file.Read(buffer, sizeof(buffer))) > 0) {
    contents->append(buffer, length);
  }
  return length >= 0;
}

void SubtractTimespec(const timespec& t1, const timespec& t2,
                      timespec* result) {
  result->tv_sec = t1.tv_sec - t2.tv_sec;
  result->tv_nsec = t1.tv_nsec - t2.tv_nsec;
  if (result->tv_nsec < 0) {
    result->tv_sec -= 1;
    result->tv_nsec += static_cast<long>(1E9);
  }
}

void TimespecToTimeval(const timespec& ts, timeval* tv) {
  tv->tv_sec = ts.tv_sec;
  tv->tv_usec = ts.tv_nsec / 1000;
}

class ScopedPtraceDetach {
 public:
  explicit ScopedPtraceDetach(pid_t pid) : pid_(pid) {}
  ~ScopedPtraceDetach() {
    if (ptrace(PTRACE_DETACH, pid_, nullptr, nullptr) != 0) {
      PLOG(ERROR) << "ptrace";
    }
  }

 private:
  pid_t pid_;

  DISALLOW_COPY_AND_ASSIGN(ScopedPtraceDetach);
};

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
      start_time_initialized_(),
      is_64_bit_initialized_(),
      is_64_bit_(false),
      initialized_() {}

ProcessInfo::~ProcessInfo() {}

bool ProcessInfo::Initialize(pid_t pid) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  pid_ = pid;

  {
    char path[32];
    snprintf(path, sizeof(path), "/proc/%d/status", pid_);
    base::ScopedFILE status_file(fopen(path, "re"));
    if (!status_file.get()) {
      PLOG(ERROR) << "fopen " << path;
      return false;
    }

    size_t buffer_size = 0;
    char* buffer = nullptr;
    ScopedBufferPtr buffer_owner(&buffer);

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
  // TODO(jperaza): Is this possible to determine?
  return false;
}

bool ProcessInfo::Is64Bit(bool* is_64_bit) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  if (is_64_bit_initialized_.is_uninitialized()) {
    is_64_bit_initialized_.set_invalid();

#if defined(ARCH_CPU_64_BITS)
    const bool am_64_bit = true;
#else
    const bool am_64_bit = false;
#endif

    if (pid_ == getpid()) {
      is_64_bit_ = am_64_bit;
    } else {
      if (ptrace(PTRACE_ATTACH, pid_, nullptr, nullptr) != 0) {
        PLOG(ERROR) << "ptrace";
        return false;
      }

      ScopedPtraceDetach ptrace_detach(pid_);

      if (HANDLE_EINTR(waitpid(pid_, nullptr, __WALL)) < 0) {
        PLOG(ERROR) << "waitpid";
        return false;
      }

      // Allocate more buffer space than is required to hold registers for this
      // process. If the kernel fills the extra space, the target process uses
      // more/larger registers than this process. If the kernel fills less space
      // than sizeof(regs) then the target process uses smaller/fewer registers.
      struct {
#if defined(ARCH_CPU_X86_FAMILY)
        using PrStatusType = user_regs_struct;
#elif defined(ARCH_CPU_ARMEL)
        using PrStatusType = pt_regs;
#elif defined(ARCH_CPU_ARM64)
        using PrStatusType = user_pt_regs;
#endif
        PrStatusType regs;
        char extra;
      } regbuf;

      iovec iov;
      iov.iov_base = &regbuf;
      iov.iov_len = sizeof(regbuf);
      if (ptrace(PTRACE_GETREGSET,
                 pid_,
                 reinterpret_cast<void*>(NT_PRSTATUS),
                 &iov) != 0) {
        PLOG(ERROR) << "ptrace";
        return false;
      }

      is_64_bit_ = am_64_bit == (iov.iov_len == sizeof(regbuf.regs));
    }

    is_64_bit_initialized_.set_valid();
  }

  if (!is_64_bit_initialized_.is_valid()) {
    return false;
  }

  *is_64_bit = is_64_bit_;
  return true;
}

bool ProcessInfo::StartTime(timeval* start_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  if (start_time_initialized_.is_uninitialized()) {
    start_time_initialized_.set_invalid();

    char path[32];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid_);
    std::string stat_contents;
    if (!ReadEntireFile(path, &stat_contents)) {
      return false;
    }

    // The process start time is the 22nd column.
    // The second column is the executable name in parentheses.
    // The executable name may have parentheses itself, so find the end of the
    // second column by working backwards to find the last closing parens and
    // then count forward to the 22nd column.
    size_t stat_pos = stat_contents.rfind(')');
    if (stat_pos == std::string::npos) {
      LOG(ERROR) << "format error";
      return false;
    }

    for (int index = 1; index < 21; ++index) {
      stat_pos = stat_contents.find(' ', stat_pos);
      if (stat_pos == std::string::npos) {
        break;
      }
      ++stat_pos;
    }
    if (stat_pos >= stat_contents.size()) {
      LOG(ERROR) << "format error";
      return false;
    }

    const char* ticks_ptr = &stat_contents[stat_pos];

    // start time is in jiffies instead of clock ticks pre 2.6.
    uint64_t ticks_after_boot;
    if (!AdvancePastNumber<uint64_t>(&ticks_ptr, &ticks_after_boot)) {
      LOG(ERROR) << "format error";
      return false;
    }
    long clock_ticks_per_s = sysconf(_SC_CLK_TCK);
    if (clock_ticks_per_s <= 0) {
      PLOG(ERROR) << "sysconf";
      return false;
    }
    timeval time_after_boot;
    time_after_boot.tv_sec = ticks_after_boot / clock_ticks_per_s;
    time_after_boot.tv_usec =
        (ticks_after_boot % clock_ticks_per_s) *
        (static_cast<long>(1E6) / clock_ticks_per_s);

    timespec uptime;
    if (clock_gettime(CLOCK_BOOTTIME, &uptime) != 0) {
      PLOG(ERROR) << "clock_gettime";
      return false;
    }

    timespec current_time;
    if (clock_gettime(CLOCK_REALTIME, &current_time) != 0) {
      PLOG(ERROR) << "clock_gettime";
      return false;
    }

    timespec boot_time_ts;
    SubtractTimespec(current_time, uptime, &boot_time_ts);
    timeval boot_time_tv;
    TimespecToTimeval(boot_time_ts, &boot_time_tv);
    timeradd(&boot_time_tv, &time_after_boot, &start_time_);

    start_time_initialized_.set_valid();
  }

  if (!start_time_initialized_.is_valid()) {
    return false;
  }

  *start_time = start_time_;
  return true;
}

bool ProcessInfo::Arguments(std::vector<std::string>* argv) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  char path[32];
  snprintf(path, sizeof(path), "/proc/%d/cmdline", pid_);
  std::string command;
  if (!ReadEntireFile(path, &command)) {
    return false;
  }

  if (command.size() == 0 || command.back() != '\0') {
    LOG(ERROR) << "format error";
    return false;
  }

  command.pop_back();
  std::vector<std::string> local_argv = SplitString(command, '\0');
  argv->swap(local_argv);
  return true;
}

}  // namespace crashpad
