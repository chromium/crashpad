// Copyright 2014 The Crashpad Authors. All rights reserved.
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

#include <stdio.h>
#include <string>
#include <sys/utsname.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/files/scoped_file.h"
#include "util/file/file_reader.h"

namespace crashpad {

ProcessInfo::ProcessInfo() : initialized_() {
}

ProcessInfo::~ProcessInfo() {
}

static void advance_to_whitespace(char** line_ptr) {
  while (**line_ptr != '\t' && **line_ptr != ' ' && **line_ptr != '\n') {
    ++(*line_ptr);
  }
}

#define CHECK_FILE(file, path) \
    if (file.get() == NULL) { \
      LOG(ERROR) << "Unable to open file: " << path; \
      return false; \
    }

bool ProcessInfo::Initialize(pid_t pid) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  pid_ = pid;

  char path[32];

  sprintf(path, "/proc/%d/status", pid);
  {
    base::ScopedFILE status_file(fopen(path, "r"));
    CHECK_FILE(status_file, path);
    WeakStdioFileLineReader reader(status_file.get());
    char* line = NULL;
    while (reader.GetLine(&line) > 0) {
      if (strncmp(line, "PPid:", 5) == 0) {
        sscanf(line, "PPid:\t%d", &ppid_);
      } else if (strncmp(line, "Uid:", 4) == 0) {
        sscanf(line, "Uid:\t%d\t%d\t%d", &uid_, &euid_, &suid_);
      } else if (strncmp(line, "Gid:", 4) == 0) {
        sscanf(line, "Gid:\t%d\t%d\t%d", &gid_, &egid_, &sgid_);
      } else if (strncmp(line, "Groups:", 7) == 0) {
        gid_t group;
        char* line_ptr = line;
        do {
          advance_to_whitespace(&line_ptr);
          if (sscanf(line_ptr, "\t%d", &group) != 1) {
            break;
          }
          supplementary_groups_.insert(group);
          ++line_ptr;
        } while(true);
      }
    }
    if (ppid_ == 0 || uid_ == 0 || gid_ == 0) {
      LOG(ERROR) << "Missing fields in status file";
      return false;
    }
  }

  unsigned long long boot_time_s = 0;
  sprintf(path, "/proc/stat");
  {
    base::ScopedFILE stat_file(fopen(path, "r"));
    CHECK_FILE(stat_file, path);
    WeakStdioFileLineReader reader(stat_file.get());
    char* line = NULL;
    while (reader.GetLine(&line) > 0) {
      if (strncmp(line, "btime", 5) == 0) {
        sscanf(line, "btime %lld", &boot_time_s);
        break;
      }
    }
    if (boot_time_s == 0) {
      LOG(ERROR) << "Unable to read boot time from /proc/stat";
      return false;
    }
  }

  sprintf(path, "/proc/%d/stat", pid);
  {
    base::ScopedFILE stat_file(fopen(path, "r"));
    CHECK_FILE(stat_file, path);
    WeakStdioFileLineReader reader(stat_file.get());
    char* line = NULL;
    ssize_t line_length = reader.GetLine(&line);
    if (line_length <= 0) {
      LOG(ERROR) << "Couldn't get line from stat file";
      return false;
    }
    // The process start time is the 22nd column in the line.
    // The second column is the executable name in parentheses.
    // The executable name may have parentheses itself, so find the third
    // column by working backwards to find the last closing parens and then
    // count forward to the 22nd column.
    char* line_ptr = line + line_length;
    while (line_ptr > line + 1 && *(line_ptr-1) != ')') {
      --line_ptr;
    }
    for (int index = 2; index < 22 && line_ptr < line + line_length; ++index) {
      ++line_ptr;
      advance_to_whitespace(&line_ptr);
    }

    // TODO(jperaza) start time is in jiffies instead of clock ticks pre 2.6.
    unsigned long long ticks_after_boot;
    sscanf(line_ptr, "%lld", &ticks_after_boot);

    long clock_ticks_per_s = sysconf(_SC_CLK_TCK);
    if (clock_ticks_per_s <= 0) {
      LOG(ERROR) << "Couldn't get system clock tick rate";
      return false;
    }
    start_time_.tv_usec = 0;
    start_time_.tv_sec = boot_time_s + (ticks_after_boot / clock_ticks_per_s);
  }

  sprintf(path, "/proc/%d/exe", pid);
  {
    base::ScopedFILE exe_file(fopen(path, "r"));
    CHECK_FILE(exe_file, path);

    char header[4];
    if (fread(header, 1, 4, exe_file.get()) != 4) {
      LOG(ERROR) << "Couldn't read file header";
      return false;
    }
    if (header[0] != 0x7f || header[1] != 0x45 || header[2] != 0x4c ||
        header[3] != 0x46) {
      LOG(ERROR) << "Unknown executable file type:" << std::hex
                 << header[0] << header[1] << header[2] << header[3];
      return false;
    }
    char bit_format;
    if (fread(&bit_format, 1, 1, exe_file.get()) != 1) {
      LOG(ERROR) << "Couldn't read exe bit format";
      return false;
    }
    is_64_bit_ = bit_format == 2;
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

static void advance_past_null(char** line_ptr) {
  while (**line_ptr != '\0') {
    ++(*line_ptr);
  }
  ++(*line_ptr);
}

bool ProcessInfo::Arguments(std::vector<std::string>* argv) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  char path[32];
  std::vector<std::string> local_argv;
  sprintf(path, "/proc/%d/cmdline", pid_);
  {
    base::ScopedFILE cmd_file(fopen(path, "r"));
    CHECK_FILE(cmd_file, path);
    WeakStdioFileLineReader reader(cmd_file.get());
    char* line;
    size_t line_length = reader.GetLine(&line);
    if (line_length <= 0) {
      LOG(ERROR) << "Couldn't read cmdline";
      return false;
    }
    for (char* line_ptr = line; line_ptr < line + line_length;
         advance_past_null(&line_ptr)) {
      local_argv.push_back(line_ptr);
    }
  }
  argv->swap(local_argv);
  return true;
}

}  // namespace crashpad
