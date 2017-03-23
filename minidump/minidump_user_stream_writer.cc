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

#include "base/memory/ptr_util.h"
#include "util/file/file_writer.h"

namespace crashpad {

class MinidumpUserStreamWriter::ContentsWriter {
 public:
  virtual ~ContentsWriter() {}
  virtual bool WriteContents(FileWriterInterface* writer) = 0;
  virtual size_t GetSize() const = 0;
};

class MinidumpUserStreamWriter::SnapshotContentsWriter final
    : public MinidumpUserStreamWriter::ContentsWriter,
      public MemorySnapshot::Delegate {
 public:
  explicit SnapshotContentsWriter(const MemorySnapshot* snapshot)
      : snapshot_(snapshot), writer_(nullptr) {}

  bool WriteContents(FileWriterInterface* writer) override {
    DCHECK(!writer_);

    writer_ = writer;
    if (!snapshot_)
      return true;

    return snapshot_->Read(this);
  }

  size_t GetSize() const override { return snapshot_ ? snapshot_->Size() : 0; };

  bool MemorySnapshotDelegateRead(void* data, size_t size) override {
    return writer_->Write(data, size);
  }

 private:
  const MemorySnapshot* snapshot_;
  FileWriterInterface* writer_;

  DISALLOW_COPY_AND_ASSIGN(SnapshotContentsWriter);
};

class MinidumpUserStreamWriter::BufferContentsWriter final
    : public MinidumpUserStreamWriter::ContentsWriter {
 public:
  BufferContentsWriter(const void* buffer, size_t buffer_size)
      : buffer_(buffer), buffer_size_(buffer_size) {}

  bool WriteContents(FileWriterInterface* writer) override {
    return writer->Write(buffer_, buffer_size_);
  }
  size_t GetSize() const override { return buffer_size_; }

 private:
  const void* buffer_;
  size_t buffer_size_;

  DISALLOW_COPY_AND_ASSIGN(BufferContentsWriter);
};

MinidumpUserStreamWriter::MinidumpUserStreamWriter() : stream_type_() {}

MinidumpUserStreamWriter::~MinidumpUserStreamWriter() {
}

void MinidumpUserStreamWriter::InitializeFromSnapshot(
    const UserMinidumpStream* stream) {
  DCHECK_EQ(state(), kStateMutable);
  DCHECK(!contents_writer_.get());

  stream_type_ = static_cast<MinidumpStreamType>(stream->stream_type());
  contents_writer_ =
      base::WrapUnique(new SnapshotContentsWriter(stream->memory()));
}

void MinidumpUserStreamWriter::InitializeFromBuffer(
    MinidumpStreamType stream_type,
    const void* buffer,
    size_t buffer_size) {
  DCHECK_EQ(state(), kStateMutable);
  DCHECK(!contents_writer_.get());

  stream_type_ = stream_type;
  contents_writer_ =
      base::WrapUnique(new BufferContentsWriter(buffer, buffer_size));
}

bool MinidumpUserStreamWriter::Freeze() {
  DCHECK_EQ(state(), kStateMutable);
  DCHECK_NE(stream_type_, 0u);
  return MinidumpStreamWriter::Freeze();
}

size_t MinidumpUserStreamWriter::SizeOfObject() {
  DCHECK_GE(state(), kStateFrozen);

  return contents_writer_->GetSize();
}

std::vector<internal::MinidumpWritable*>
MinidumpUserStreamWriter::Children() {
  DCHECK_GE(state(), kStateFrozen);
  return std::vector<internal::MinidumpWritable*>();
}

bool MinidumpUserStreamWriter::WriteObject(FileWriterInterface* file_writer) {
  DCHECK_EQ(state(), kStateWritable);

  return contents_writer_->WriteContents(file_writer);
}

MinidumpStreamType MinidumpUserStreamWriter::StreamType() const {
  return static_cast<MinidumpStreamType>(stream_type_);
}

}  // namespace crashpad
