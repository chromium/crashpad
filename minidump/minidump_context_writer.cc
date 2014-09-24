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

#include "minidump/minidump_context_writer.h"

#include "base/logging.h"

namespace crashpad {

MinidumpContextWriter::~MinidumpContextWriter() {
}

size_t MinidumpContextWriter::SizeOfObject() {
  DCHECK_GE(state(), kStateFrozen);

  return ContextSize();
}

MinidumpContextX86Writer::MinidumpContextX86Writer()
    : MinidumpContextWriter(), context_() {
  context_.context_flags = kMinidumpContextX86;
}

MinidumpContextX86Writer::~MinidumpContextX86Writer() {
}

bool MinidumpContextX86Writer::WriteObject(FileWriterInterface* file_writer) {
  DCHECK_EQ(state(), kStateWritable);

  return file_writer->Write(&context_, sizeof(context_));
}

size_t MinidumpContextX86Writer::ContextSize() const {
  DCHECK_GE(state(), kStateFrozen);

  return sizeof(context_);
}

MinidumpContextAMD64Writer::MinidumpContextAMD64Writer()
    : MinidumpContextWriter(), context_() {
  context_.context_flags = kMinidumpContextAMD64;
}

MinidumpContextAMD64Writer::~MinidumpContextAMD64Writer() {
}

size_t MinidumpContextAMD64Writer::Alignment() {
  DCHECK_GE(state(), kStateFrozen);

  // Match the alignment of MinidumpContextAMD64.
  return 16;
}

bool MinidumpContextAMD64Writer::WriteObject(FileWriterInterface* file_writer) {
  DCHECK_EQ(state(), kStateWritable);

  return file_writer->Write(&context_, sizeof(context_));
}

size_t MinidumpContextAMD64Writer::ContextSize() const {
  DCHECK_GE(state(), kStateFrozen);

  return sizeof(context_);
}

}  // namespace crashpad
