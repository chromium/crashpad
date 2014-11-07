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
#include "snapshot/module_snapshot.h"
#include "util/file/file_writer.h"
#include "util/numeric/safe_assignment.h"

namespace crashpad {

MinidumpModuleCrashpadInfoWriter::MinidumpModuleCrashpadInfoWriter()
    : MinidumpWritable(),
      module_(),
      list_annotations_(),
      simple_annotations_() {
  module_.version = MinidumpModuleCrashpadInfo::kVersion;
}

MinidumpModuleCrashpadInfoWriter::~MinidumpModuleCrashpadInfoWriter() {
}

void MinidumpModuleCrashpadInfoWriter::InitializeFromSnapshot(
    const ModuleSnapshot* module_snapshot, size_t module_list_index) {
  DCHECK_EQ(state(), kStateMutable);
  DCHECK(!list_annotations_);
  DCHECK(!simple_annotations_);

  uint32_t module_list_index_u32;
  if (!AssignIfInRange(&module_list_index_u32, module_list_index)) {
    LOG(ERROR) << "module_list_index " << module_list_index << " out of range";
    return;
  }
  SetMinidumpModuleListIndex(module_list_index_u32);

  auto list_annotations = make_scoped_ptr(new MinidumpUTF8StringListWriter());
  list_annotations->InitializeFromVector(module_snapshot->AnnotationsVector());
  if (list_annotations->IsUseful()) {
    SetListAnnotations(list_annotations.Pass());
  }

  auto simple_annotations =
      make_scoped_ptr(new MinidumpSimpleStringDictionaryWriter());
  simple_annotations->InitializeFromMap(
      module_snapshot->AnnotationsSimpleMap());
  if (simple_annotations->IsUseful()) {
    SetSimpleAnnotations(simple_annotations.Pass());
  }
}

void MinidumpModuleCrashpadInfoWriter::SetListAnnotations(
    scoped_ptr<MinidumpUTF8StringListWriter> list_annotations) {
  DCHECK_EQ(state(), kStateMutable);

  list_annotations_ = list_annotations.Pass();
}

void MinidumpModuleCrashpadInfoWriter::SetSimpleAnnotations(
    scoped_ptr<MinidumpSimpleStringDictionaryWriter> simple_annotations) {
  DCHECK_EQ(state(), kStateMutable);

  simple_annotations_ = simple_annotations.Pass();
}

bool MinidumpModuleCrashpadInfoWriter::IsUseful() const {
  return list_annotations_ || simple_annotations_;
}

bool MinidumpModuleCrashpadInfoWriter::Freeze() {
  DCHECK_EQ(state(), kStateMutable);

  if (!MinidumpWritable::Freeze()) {
    return false;
  }

  if (list_annotations_) {
    list_annotations_->RegisterLocationDescriptor(&module_.list_annotations);
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
  if (list_annotations_) {
    children.push_back(list_annotations_.get());
  }
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
    : MinidumpLocationDescriptorListWriter() {
}

MinidumpModuleCrashpadInfoListWriter::~MinidumpModuleCrashpadInfoListWriter() {
}

void MinidumpModuleCrashpadInfoListWriter::InitializeFromSnapshot(
    const std::vector<const ModuleSnapshot*>& module_snapshots) {
  DCHECK_EQ(state(), kStateMutable);
  DCHECK(IsEmpty());
  DCHECK(child_location_descriptors().empty());

  size_t count = module_snapshots.size();
  for (size_t index = 0; index < count; ++index) {
    const ModuleSnapshot* module_snapshot = module_snapshots[index];

    auto module = make_scoped_ptr(new MinidumpModuleCrashpadInfoWriter());
    module->InitializeFromSnapshot(module_snapshot, index);
    if (module->IsUseful()) {
      AddModule(module.Pass());
    }
  }
}

void MinidumpModuleCrashpadInfoListWriter::AddModule(
    scoped_ptr<MinidumpModuleCrashpadInfoWriter> module) {
  DCHECK_EQ(state(), kStateMutable);

  AddChild(module.Pass());
}

bool MinidumpModuleCrashpadInfoListWriter::IsUseful() const {
  return !IsEmpty();
}

}  // namespace crashpad
