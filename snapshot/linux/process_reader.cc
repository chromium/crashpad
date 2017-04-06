
#include "snapshot/linux/process_reader.h"

#include <dirent.h>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "util/posix/scoped_dir.h"

#include <memory>

namespace crashpad {

ProcessReader::ProcessReader()
    : process_info_(),
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
  if (initialized_threads_) {
    return threads_;
  }

  DCHECK(threads_.empty());

  // Collect thread ids
  char path[32];
  snprintf(path, sizeof(path), "/proc/%d/task", ProcessID());
  DIR* dir = opendir(path);
  if (!dir) {
    PLOG(ERROR) << "opendir";
    return threads_;
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

  // Collect thread contexts

  // Determine stack locations

  return threads_;
}

}  // namespace crashpad
