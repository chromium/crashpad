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

#include "minidump/minidump_crashpad_info_writer.h"

#include "base/logging.h"
#include "util/file/file_writer.h"

namespace crashpad {

MinidumpCrashpadInfoWriter::MinidumpCrashpadInfoWriter()
    : MinidumpStreamWriter(), crashpad_info_(), simple_annotations_() {
  crashpad_info_.size = sizeof(crashpad_info_);
  crashpad_info_.version = 1;
}

MinidumpCrashpadInfoWriter::~MinidumpCrashpadInfoWriter() {
}

void MinidumpCrashpadInfoWriter::SetSimpleAnnotations(
    MinidumpSimpleStringDictionaryWriter* simple_annotations) {
  DCHECK_EQ(state(), kStateMutable);

  simple_annotations_ = simple_annotations;
}

bool MinidumpCrashpadInfoWriter::Freeze() {
  DCHECK_EQ(state(), kStateMutable);

  if (!MinidumpStreamWriter::Freeze()) {
    return false;
  }

  if (simple_annotations_) {
    simple_annotations_->RegisterLocationDescriptor(
        &crashpad_info_.simple_annotations);
  }

  return true;
}

size_t MinidumpCrashpadInfoWriter::SizeOfObject() {
  DCHECK_GE(state(), kStateFrozen);

  return sizeof(crashpad_info_);
}

std::vector<internal::MinidumpWritable*>
MinidumpCrashpadInfoWriter::Children() {
  DCHECK_GE(state(), kStateFrozen);

  std::vector<MinidumpWritable*> children;
  if (simple_annotations_) {
    children.push_back(simple_annotations_);
  }

  return children;
}

bool MinidumpCrashpadInfoWriter::WriteObject(FileWriterInterface* file_writer) {
  DCHECK_EQ(state(), kStateWritable);

  return file_writer->Write(&crashpad_info_, sizeof(crashpad_info_));
}

MinidumpStreamType MinidumpCrashpadInfoWriter::StreamType() const {
  return kMinidumpStreamTypeCrashpadInfo;
}

}  // namespace crashpad
