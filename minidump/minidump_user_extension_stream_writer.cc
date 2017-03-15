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

#include "minidump/minidump_user_extension_stream_writer.h"

#include "minidump/minidump_user_extension_stream.h"
#include "util/file/file_writer.h"

namespace crashpad {

MinidumpUserExtensionStreamWriter::MinidumpUserExtensionStreamWriter()
    : stream_type_(0) {}

MinidumpUserExtensionStreamWriter::~MinidumpUserExtensionStreamWriter() {}

void MinidumpUserExtensionStreamWriter::InitializeFromSnapshot(
    const MinidumpUserExtensionStream* stream) {
  DCHECK_EQ(state(), kStateMutable);

  stream_type_ = stream->stream_type();

  // TODO(siggi): The data!
}

bool MinidumpUserExtensionStreamWriter::Freeze() {
  DCHECK_EQ(state(), kStateMutable);
  DCHECK_NE(stream_type_, 0u);
  return MinidumpStreamWriter::Freeze();
}

size_t MinidumpUserExtensionStreamWriter::SizeOfObject() {
  DCHECK_GE(state(), kStateFrozen);
  return 0;  // DO NOT SUBMIT
}

std::vector<internal::MinidumpWritable*>
MinidumpUserExtensionStreamWriter::Children() {
  DCHECK_GE(state(), kStateFrozen);
  return std::vector<internal::MinidumpWritable*>();
}

bool MinidumpUserExtensionStreamWriter::WriteObject(
    FileWriterInterface* file_writer) {
  DCHECK_EQ(state(), kStateWritable);
  return true;  // DO NOT SUBMIT file_writer->Write(reader_.data(),
                // reader_.size());
}

MinidumpStreamType MinidumpUserExtensionStreamWriter::StreamType() const {
  return static_cast<MinidumpStreamType>(stream_type_);
}

}  // namespace crashpad
