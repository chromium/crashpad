// Copyright 2019 The Crashpad Authors. All rights reserved.
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

#include "snapshot/minidump/exception_snapshot_minidump.h"

#include "snapshot/minidump/minidump_string_reader.h"

namespace crashpad {
namespace internal {

ExceptionSnapshotMinidump::ExceptionSnapshotMinidump()
    : ExceptionSnapshot(),
      minidump_exception_stream_(),
      exception_information_(),
      valid_() {}

ExceptionSnapshotMinidump::~ExceptionSnapshotMinidump() {}

bool ExceptionSnapshotMinidump::Initialize(FileReaderInterface* file_reader,
                                           RVA minidump_exception_stream_rva) {
  if (!file_reader->SeekSet(minidump_exception_stream_rva)) {
    return false;
  }

  if (!file_reader->ReadExactly(&minidump_exception_stream_,
                                sizeof(minidump_exception_stream_))) {
    return false;
  }

  const size_t num_parameters =
      minidump_exception_stream_.ExceptionRecord.NumberParameters;
  for (size_t i = 0; i < num_parameters; ++i) {
    exception_information_.push_back(
        minidump_exception_stream_.ExceptionRecord.ExceptionInformation[i]);
  }

  valid_ = true;
  return true;
}

const CPUContext* ExceptionSnapshotMinidump::Context() const {
  DCHECK(valid_);
  NOTREACHED();  // https://crashpad.chromium.org/bug/10
  return nullptr;
}

uint64_t ExceptionSnapshotMinidump::ThreadID() const {
  DCHECK(valid_);
  return minidump_exception_stream_.ThreadId;
}

uint32_t ExceptionSnapshotMinidump::Exception() const {
  DCHECK(valid_);
  return minidump_exception_stream_.ExceptionRecord.ExceptionCode;
}

uint32_t ExceptionSnapshotMinidump::ExceptionInfo() const {
  DCHECK(valid_);
  return minidump_exception_stream_.ExceptionRecord.ExceptionFlags;
}

uint64_t ExceptionSnapshotMinidump::ExceptionAddress() const {
  DCHECK(valid_);
  return minidump_exception_stream_.ExceptionRecord.ExceptionAddress;
}

const std::vector<uint64_t>& ExceptionSnapshotMinidump::Codes() const {
  DCHECK(valid_);
  return exception_information_;
}

std::vector<const MemorySnapshot*> ExceptionSnapshotMinidump::ExtraMemory()
    const {
  DCHECK(valid_);
  return std::vector<const MemorySnapshot*>();
}

}  // namespace internal
}  // namespace crashpad
