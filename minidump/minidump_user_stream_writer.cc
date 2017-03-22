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

class MinidumpUserStreamWriter::MemoryReader : public MemorySnapshot::Delegate {
 public:
  MemoryReader(FileWriterInterface* writer) : writer_(writer) {}
  ~MemoryReader() override;

  bool MemorySnapshotDelegateRead(void* data, size_t size) override;

 private:
  FileWriterInterface* writer_;
};

MinidumpUserStreamWriter::MinidumpUserStreamWriter()
    : stream_type_(0), stream_size_(0), snapshot_(nullptr), buffer_(nullptr) {}

MinidumpUserStreamWriter::~MinidumpUserStreamWriter() {
}

void MinidumpUserStreamWriter::InitializeFromSnapshot(
    const UserMinidumpStream* stream) {
  DCHECK_EQ(state(), kStateMutable);
  DCHECK(!snapshot_);
  DCHECK(!buffer_);
  DCHECK(stream);

  stream_type_ = stream->stream_type();
  snapshot_ = stream->memory();
  if (snapshot_)
    stream_size_ = snapshot_->Size();
}

void MinidumpUserStreamWriter::InitializeFromBuffer(uint32_t stream_type,
                                                    const void* buffer,
                                                    size_t buffer_size) {
  DCHECK_EQ(state(), kStateMutable);
  DCHECK(!snapshot_);
  DCHECK(!buffer_);
  DCHECK(buffer_size == 0 || buffer);

  stream_type_ = stream_type;
  stream_size_ = buffer_size;
  buffer_ = buffer;
}

bool MinidumpUserStreamWriter::Freeze() {
  DCHECK_EQ(state(), kStateMutable);
  DCHECK_NE(stream_type_, 0u);
  return MinidumpStreamWriter::Freeze();
}

size_t MinidumpUserStreamWriter::SizeOfObject() {
  DCHECK_GE(state(), kStateFrozen);

  return stream_size_;
}

std::vector<internal::MinidumpWritable*>
MinidumpUserStreamWriter::Children() {
  DCHECK_GE(state(), kStateFrozen);
  return std::vector<internal::MinidumpWritable*>();
}

bool MinidumpUserStreamWriter::WriteObject(FileWriterInterface* file_writer) {
  DCHECK_EQ(state(), kStateWritable);

  // The empty stream is trivial.
  if (!stream_size_)
    return true;

  // A non-empty stream needs one of the two sources.
  DCHECK(buffer_ || snapshot_);
  DCHECK(!(buffer_ && snapshot_));

  if (buffer_)
    return file_writer->Write(buffer_, stream_size_);

  MemoryReader reader(file_writer);
  return snapshot_->Read(&reader);
}

MinidumpStreamType MinidumpUserStreamWriter::StreamType() const {
  return static_cast<MinidumpStreamType>(stream_type_);
}

MinidumpUserStreamWriter::MemoryReader::~MemoryReader() {}

bool MinidumpUserStreamWriter::MemoryReader::MemorySnapshotDelegateRead(
    void* data,
    size_t size) {
  return writer_->Write(data, size);
}

}  // namespace crashpad
