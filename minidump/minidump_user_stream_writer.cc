// Copyright 2016 The Crashpad Authors. All rights reserved.
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

#include "minidump/minidump_user_stream_writer.h"

#include "util/file/file_writer.h"

namespace crashpad {

MinidumpUserStreamWriter::MinidumpUserStreamWriter()
    : stream_type_(0), reader_() {
}

MinidumpUserStreamWriter::~MinidumpUserStreamWriter() {
}

void MinidumpUserStreamWriter::InitializeFromSnapshot(
    const UserMinidumpStream* stream) {
  DCHECK_EQ(state(), kStateMutable);

  stream_type_ = stream->stream_type();
  if (stream->memory())
    stream->memory()->Read(&reader_);
}

bool MinidumpUserStreamWriter::Freeze() {
  DCHECK_EQ(state(), kStateMutable);
  DCHECK_NE(stream_type_, 0u);
  return MinidumpStreamWriter::Freeze();
}

size_t MinidumpUserStreamWriter::SizeOfObject() {
  DCHECK_GE(state(), kStateFrozen);
  return reader_.size();
}

std::vector<internal::MinidumpWritable*>
MinidumpUserStreamWriter::Children() {
  DCHECK_GE(state(), kStateFrozen);
  return std::vector<internal::MinidumpWritable*>();
}

bool MinidumpUserStreamWriter::WriteObject(FileWriterInterface* file_writer) {
  DCHECK_EQ(state(), kStateWritable);
  return file_writer->Write(reader_.data(), reader_.size());
}

MinidumpStreamType MinidumpUserStreamWriter::StreamType() const {
  return static_cast<MinidumpStreamType>(stream_type_);
}

MinidumpUserStreamWriter::MemoryReader::~MemoryReader() {}

bool MinidumpUserStreamWriter::MemoryReader::MemorySnapshotDelegateRead(
    void* data,
    size_t size) {
  data_.resize(size);
  memcpy(&data_[0], data, size);
  return true;
}

}  // namespace crashpad
