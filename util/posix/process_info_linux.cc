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
#include <errno.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/uio.h>
#include <sys/user.h>
#include <time.h>
#include <unistd.h>

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "util/file/delimited_file_reader.h"
#include "util/file/file_reader.h"
#include "util/linux/scoped_ptrace_attach.h"

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
    FileReader status_file;
    if (!status_file.Open(base::FilePath(path))) {
      return false;
    }

    DelimitedFileReader status_file_line_reader(&status_file);

    bool have_ppid = false;
    bool have_uids = false;
    bool have_gids = false;
    bool have_groups = false;
    std::string line;
    DelimitedFileReader::Result result;
    while ((result = status_file_line_reader.GetLine(&line)) ==
           DelimitedFileReader::Result::kSuccess) {
      if (line.back() != '\n') {
        LOG(ERROR) << "format error: unterminated line at EOF";
        return false;
      }

      bool understood_line = false;
      const char* line_c = line.c_str();
      if (AdvancePastPrefix(&line_c, "PPid:\t")) {
        if (have_ppid) {
          LOG(ERROR) << "format error: multiple PPid lines";
          return false;
        }
        have_ppid = AdvancePastNumber(&line_c, &ppid_);
        if (!have_ppid) {
          LOG(ERROR) << "format error: unrecognized PPid format";
          return false;
        }
        understood_line = true;
      } else if (AdvancePastPrefix(&line_c, "Uid:\t")) {
        if (have_uids) {
          LOG(ERROR) << "format error: multiple Uid lines";
          return false;
        }
        uid_t fsuid;
        have_uids = AdvancePastNumber(&line_c, &uid_) &&
                    AdvancePastPrefix(&line_c, "\t") &&
                    AdvancePastNumber(&line_c, &euid_) &&
                    AdvancePastPrefix(&line_c, "\t") &&
                    AdvancePastNumber(&line_c, &suid_) &&
                    AdvancePastPrefix(&line_c, "\t") &&
                    AdvancePastNumber(&line_c, &fsuid);
        if (!have_uids) {
          LOG(ERROR) << "format error: unrecognized Uid format";
          return false;
        }
        understood_line = true;
      } else if (AdvancePastPrefix(&line_c, "Gid:\t")) {
        if (have_gids) {
          LOG(ERROR) << "format error: multiple Gid lines";
          return false;
        }
        gid_t fsgid;
        have_gids = AdvancePastNumber(&line_c, &gid_) &&
                    AdvancePastPrefix(&line_c, "\t") &&
                    AdvancePastNumber(&line_c, &egid_) &&
                    AdvancePastPrefix(&line_c, "\t") &&
                    AdvancePastNumber(&line_c, &sgid_) &&
                    AdvancePastPrefix(&line_c, "\t") &&
                    AdvancePastNumber(&line_c, &fsgid);
        if (!have_gids) {
          LOG(ERROR) << "format error: unrecognized Gid format";
          return false;
        }
        understood_line = true;
      } else if (AdvancePastPrefix(&line_c, "Groups:\t")) {
        if (have_groups) {
          LOG(ERROR) << "format error: multiple Groups lines";
          return false;
        }
        gid_t group;
        while (AdvancePastNumber(&line_c, &group)) {
          supplementary_groups_.insert(group);
          if (!AdvancePastPrefix(&line_c, " ")) {
            LOG(ERROR) << "format error: unrecognized Groups format";
            return false;
          }
        }
        have_groups = true;
        understood_line = true;
      }

      if (understood_line && line_c != &line.back()) {
        LOG(ERROR) << "format error: unconsumed trailing data";
        return false;
      }
    }
    if (result != DelimitedFileReader::Result::kEndOfFile) {
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
      ScopedPtraceAttach ptrace_attach;
      if (!ptrace_attach.ResetAttach(pid_)) {
        return false;
      }

      // Allocate more buffer space than is required to hold registers for this
      // process. If the kernel fills the extra space, the target process uses
      // more/larger registers than this process. If the kernel fills less space
      // than sizeof(regs) then the target process uses smaller/fewer registers.
      struct {
#if defined(ARCH_CPU_X86_FAMILY) || defined(ARCH_CPU_ARM64)
        using PrStatusType = user_regs_struct;
#elif defined(ARCH_CPU_ARMEL)
        using PrStatusType = user_regs;
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
        switch (errno) {
#if defined(ARCH_CPU_ARMEL)
          case EIO:
            // PTRACE_GETREGSET, introduced in Linux 2.6.34 (2225a122ae26),
            // requires kernel support enabled by HAVE_ARCH_TRACEHOOK. This has
            // been set for x86 (including x86_64) since Linux 2.6.28
            // (99bbc4b1e677a), but for ARM only since Linux 3.5.0
            // (0693bf68148c4). Fortunately, 64-bit ARM support only appeared in
            // Linux 3.7.0, so if PTRACE_GETREGSET fails on ARM with EIO,
            // indicating that the request is not supported, the kernel must be
            // old enough that 64-bit ARM isn’t supported either.
            //
            // TODO(mark): Once helpers to interpret the kernel version are
            // available, add a DCHECK to ensure that the kernel is older than
            // 3.5.
            is_64_bit_ = false;
            break;
#endif
          default:
            PLOG(ERROR) << "ptrace";
            return false;
        }
      } else {
        is_64_bit_ = am_64_bit == (iov.iov_len == sizeof(regbuf.regs));
      }
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
  FileReader cmdline_file;
  if (!cmdline_file.Open(base::FilePath(path))) {
    return false;
  }

  DelimitedFileReader cmdline_file_field_reader(&cmdline_file);

  std::vector<std::string> local_argv;
  std::string argument;
  DelimitedFileReader::Result result;
  while ((result = cmdline_file_field_reader.GetDelim('\0', &argument)) ==
         DelimitedFileReader::Result::kSuccess) {
    if (argument.back() != '\0') {
      LOG(ERROR) << "format error";
      return false;
    }
    argument.pop_back();
    local_argv.push_back(argument);
  }
  if (result != DelimitedFileReader::Result::kEndOfFile) {
    return false;
  }

  argv->swap(local_argv);
  return true;
}

}  // namespace crashpad
