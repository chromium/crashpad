// Copyright 2015 The Crashpad Authors. All rights reserved.
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

#include "snapshot/minidump/process_snapshot_minidump.h"

#include "util/file/file_io.h"
#include "snapshot/minidump/minidump_simple_string_dictionary_reader.h"

namespace crashpad {

ProcessSnapshotMinidump::ProcessSnapshotMinidump()
    : ProcessSnapshot(),
      header_(),
      stream_directory_(),
      stream_map_(),
      crashpad_info_(),
      annotations_simple_map_(),
      file_reader_(nullptr),
      initialized_() {
}

ProcessSnapshotMinidump::~ProcessSnapshotMinidump() {
}

bool ProcessSnapshotMinidump::Initialize(FileReaderInterface* file_reader) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  file_reader_ = file_reader;

  if (!file_reader_->SeekSet(0)) {
    return false;
  }

  if (!file_reader_->ReadExactly(&header_, sizeof(header_))) {
    return false;
  }

  if (header_.Signature != MINIDUMP_SIGNATURE) {
    LOG(ERROR) << "minidump signature mismatch";
    return false;
  }

  if (header_.Version != MINIDUMP_VERSION) {
    LOG(ERROR) << "minidump version mismatch";
    return false;
  }

  if (!file_reader->SeekSet(header_.StreamDirectoryRva)) {
    return false;
  }

  stream_directory_.resize(header_.NumberOfStreams);
  if (!file_reader_->ReadExactly(
          &stream_directory_[0],
          header_.NumberOfStreams * sizeof(stream_directory_[0]))) {
    return false;
  }

  for (const MINIDUMP_DIRECTORY& directory : stream_directory_) {
    const MinidumpStreamType stream_type =
        static_cast<MinidumpStreamType>(directory.StreamType);
    if (stream_map_.find(stream_type) != stream_map_.end()) {
      LOG(ERROR) << "duplicate streams for type " << directory.StreamType;
      return false;
    }

    stream_map_[stream_type] = &directory.Location;
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);

  InitializeCrashpadInfo();

  return true;
}

pid_t ProcessSnapshotMinidump::ProcessID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  NOTREACHED();
  return 0;
}

pid_t ProcessSnapshotMinidump::ParentProcessID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  NOTREACHED();
  return 0;
}

void ProcessSnapshotMinidump::SnapshotTime(timeval* snapshot_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  NOTREACHED();
  snapshot_time->tv_sec = 0;
  snapshot_time->tv_usec = 0;
}

void ProcessSnapshotMinidump::ProcessStartTime(timeval* start_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  NOTREACHED();
  start_time->tv_sec = 0;
  start_time->tv_usec = 0;
}

void ProcessSnapshotMinidump::ProcessCPUTimes(timeval* user_time,
                                              timeval* system_time) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  NOTREACHED();
  user_time->tv_sec = 0;
  user_time->tv_usec = 0;
  system_time->tv_sec = 0;
  system_time->tv_usec = 0;
}

const std::map<std::string, std::string>&
ProcessSnapshotMinidump::AnnotationsSimpleMap() const {
  // TODO(mark): This method should not be const, although the interface
  // currently imposes this requirement. Making it non-const would allow
  // annotations_simple_map_ to be lazily constructed: InitializeCrashpadInfo()
  // could be called here, and from other locations that require it, rather than
  // calling it from Initialize().
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return annotations_simple_map_;
}

const SystemSnapshot* ProcessSnapshotMinidump::System() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  NOTREACHED();
  return nullptr;
}

std::vector<const ThreadSnapshot*> ProcessSnapshotMinidump::Threads() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  NOTREACHED();
  return std::vector<const ThreadSnapshot*>();
}

std::vector<const ModuleSnapshot*> ProcessSnapshotMinidump::Modules() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  NOTREACHED();
  return std::vector<const ModuleSnapshot*>();
}

const ExceptionSnapshot* ProcessSnapshotMinidump::Exception() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  NOTREACHED();
  return nullptr;
}

void ProcessSnapshotMinidump::InitializeCrashpadInfo() {
  const auto& it = stream_map_.find(kMinidumpStreamTypeCrashpadInfo);
  if (it == stream_map_.end()) {
    return;
  }

  if (it->second->DataSize < sizeof(crashpad_info_)) {
    LOG(ERROR) << "crashpad_info size mismatch";
    return;
  }

  if (!file_reader_->SeekSet(it->second->Rva)) {
    return;
  }

  if (!file_reader_->ReadExactly(&crashpad_info_, sizeof(crashpad_info_))) {
    return;
  }

  if (crashpad_info_.version != MinidumpCrashpadInfo::kVersion) {
    LOG(ERROR) << "crashpad_info version mismatch";
    return;
  }

  internal::ReadMinidumpSimpleStringDictionary(
      file_reader_,
      crashpad_info_.simple_annotations,
      &annotations_simple_map_);
}

}  // namespace crashpad
