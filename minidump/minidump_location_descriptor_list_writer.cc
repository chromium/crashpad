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

#include "minidump/minidump_location_descriptor_list_writer.h"

#include "base/logging.h"
#include "util/file/file_writer.h"
#include "util/numeric/safe_assignment.h"

namespace crashpad {
namespace internal {

MinidumpLocationDescriptorListWriter::MinidumpLocationDescriptorListWriter()
    : MinidumpWritable(),
      location_descriptor_list_base_(),
      children_(),
      child_location_descriptors_() {
}

MinidumpLocationDescriptorListWriter::~MinidumpLocationDescriptorListWriter() {
}

void MinidumpLocationDescriptorListWriter::AddChild(
    scoped_ptr<MinidumpWritable> child) {
  DCHECK_EQ(state(), kStateMutable);

  children_.push_back(child.release());
}

bool MinidumpLocationDescriptorListWriter::Freeze() {
  DCHECK_EQ(state(), kStateMutable);
  DCHECK(child_location_descriptors_.empty());

  if (!MinidumpWritable::Freeze()) {
    return false;
  }

  size_t child_count = children_.size();
  if (!AssignIfInRange(&location_descriptor_list_base_.count, child_count)) {
    LOG(ERROR) << "child_count " << child_count << " out of range";
    return false;
  }

  child_location_descriptors_.resize(child_count);
  for (size_t index = 0; index < child_count; ++index) {
    children_[index]->RegisterLocationDescriptor(
        &child_location_descriptors_[index]);
  }

  return true;
}

size_t MinidumpLocationDescriptorListWriter::SizeOfObject() {
  DCHECK_GE(state(), kStateFrozen);

  return sizeof(location_descriptor_list_base_) +
         children_.size() * sizeof(MINIDUMP_LOCATION_DESCRIPTOR);
}

std::vector<MinidumpWritable*>
MinidumpLocationDescriptorListWriter::Children() {
  DCHECK_GE(state(), kStateFrozen);

  std::vector<MinidumpWritable*> children;
  for (MinidumpWritable* child : children_) {
    children.push_back(child);
  }

  return children;
}

bool MinidumpLocationDescriptorListWriter::WriteObject(
    FileWriterInterface* file_writer) {
  DCHECK_EQ(state(), kStateWritable);
  DCHECK_EQ(children_.size(), child_location_descriptors_.size());

  WritableIoVec iov;
  iov.iov_base = &location_descriptor_list_base_;
  iov.iov_len = sizeof(location_descriptor_list_base_);
  std::vector<WritableIoVec> iovecs(1, iov);

  if (!child_location_descriptors_.empty()) {
    iov.iov_base = &child_location_descriptors_[0];
    iov.iov_len = child_location_descriptors_.size() *
                  sizeof(MINIDUMP_LOCATION_DESCRIPTOR);
    iovecs.push_back(iov);
  }

  return file_writer->WriteIoVec(&iovecs);
}

}  // namespace internal
}  // namespace crashpad
