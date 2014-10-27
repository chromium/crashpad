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

#include "minidump/minidump_module_crashpad_info_writer.h"

#include <sys/types.h>

#include "base/logging.h"
#include "minidump/minidump_simple_string_dictionary_writer.h"
#include "util/file/file_writer.h"
#include "util/numeric/safe_assignment.h"

namespace crashpad {

MinidumpModuleCrashpadInfoWriter::MinidumpModuleCrashpadInfoWriter()
    : MinidumpWritable(), module_(), simple_annotations_() {
  module_.version = MinidumpModuleCrashpadInfo::kVersion;
}

MinidumpModuleCrashpadInfoWriter::~MinidumpModuleCrashpadInfoWriter() {
}

void MinidumpModuleCrashpadInfoWriter::SetSimpleAnnotations(
    scoped_ptr<MinidumpSimpleStringDictionaryWriter> simple_annotations) {
  DCHECK_EQ(state(), kStateMutable);

  simple_annotations_ = simple_annotations.Pass();
}

bool MinidumpModuleCrashpadInfoWriter::Freeze() {
  DCHECK_EQ(state(), kStateMutable);

  if (!MinidumpWritable::Freeze()) {
    return false;
  }

  if (simple_annotations_) {
    simple_annotations_->RegisterLocationDescriptor(
        &module_.simple_annotations);
  }

  return true;
}

size_t MinidumpModuleCrashpadInfoWriter::SizeOfObject() {
  DCHECK_GE(state(), kStateFrozen);

  return sizeof(module_);
}

std::vector<internal::MinidumpWritable*>
MinidumpModuleCrashpadInfoWriter::Children() {
  DCHECK_GE(state(), kStateFrozen);

  std::vector<MinidumpWritable*> children;
  if (simple_annotations_) {
    children.push_back(simple_annotations_.get());
  }

  return children;
}

bool MinidumpModuleCrashpadInfoWriter::WriteObject(
    FileWriterInterface* file_writer) {
  DCHECK_EQ(state(), kStateWritable);

  return file_writer->Write(&module_, sizeof(module_));
}

MinidumpModuleCrashpadInfoListWriter::MinidumpModuleCrashpadInfoListWriter()
    : MinidumpWritable(),
      module_list_base_(),
      modules_(),
      module_location_descriptors_() {
}

MinidumpModuleCrashpadInfoListWriter::~MinidumpModuleCrashpadInfoListWriter() {
}

void MinidumpModuleCrashpadInfoListWriter::AddModule(
    scoped_ptr<MinidumpModuleCrashpadInfoWriter> module) {
  DCHECK_EQ(state(), kStateMutable);

  modules_.push_back(module.release());
}

bool MinidumpModuleCrashpadInfoListWriter::Freeze() {
  DCHECK_EQ(state(), kStateMutable);
  DCHECK(module_location_descriptors_.empty());

  if (!MinidumpWritable::Freeze()) {
    return false;
  }

  size_t module_count = modules_.size();
  if (!AssignIfInRange(&module_list_base_.count, module_count)) {
    LOG(ERROR) << "module_count " << module_count << " out of range";
    return false;
  }

  module_location_descriptors_.resize(module_count);
  for (size_t index = 0; index < module_count; ++index) {
    modules_[index]->RegisterLocationDescriptor(
        &module_location_descriptors_[index]);
  }

  return true;
}

size_t MinidumpModuleCrashpadInfoListWriter::SizeOfObject() {
  DCHECK_GE(state(), kStateFrozen);

  return sizeof(module_list_base_) +
         modules_.size() * sizeof(MINIDUMP_LOCATION_DESCRIPTOR);
}

std::vector<internal::MinidumpWritable*>
MinidumpModuleCrashpadInfoListWriter::Children() {
  DCHECK_GE(state(), kStateFrozen);

  std::vector<MinidumpWritable*> children;
  for (MinidumpModuleCrashpadInfoWriter* module : modules_) {
    children.push_back(module);
  }

  return children;
}

bool MinidumpModuleCrashpadInfoListWriter::WriteObject(
    FileWriterInterface* file_writer) {
  DCHECK_EQ(state(), kStateWritable);
  DCHECK_EQ(modules_.size(), module_location_descriptors_.size());

  WritableIoVec iov;
  iov.iov_base = &module_list_base_;
  iov.iov_len = sizeof(module_list_base_);
  std::vector<WritableIoVec> iovecs(1, iov);

  if (!module_location_descriptors_.empty()) {
    iov.iov_base = &module_location_descriptors_[0];
    iov.iov_len = module_location_descriptors_.size() *
                  sizeof(MINIDUMP_LOCATION_DESCRIPTOR);
    iovecs.push_back(iov);
  }

  return file_writer->WriteIoVec(&iovecs);
}

}  // namespace crashpad
