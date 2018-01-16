// Copyright 2018 The Crashpad Authors. All rights reserved.
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

#include "snapshot/fuchsia/module_snapshot_fuchsia.h"

namespace crashpad {
namespace internal {

ModuleSnapshotFuchsia::ModuleSnapshotFuchsia() {}

ModuleSnapshotFuchsia::~ModuleSnapshotFuchsia() {}

bool ModuleSnapshotFuchsia::Initialize(const std::string& name,
                                       zx_vaddr_t base,
                                       size_t size,
                                       const std::string& buildid) {
  name_ = name;
  base_ = base;
  size_ = size;
  buildid_ = buildid;
  return true;
}

void ModuleSnapshotFuchsia::GetCrashpadOptions(
    CrashpadInfoClientOptions* options) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  process_types::CrashpadInfo crashpad_info;
  if (!mach_o_image_reader_->GetCrashpadInfo(&crashpad_info)) {
    options->crashpad_handler_behavior = TriState::kUnset;
    options->system_crash_reporter_forwarding = TriState::kUnset;
    options->gather_indirectly_referenced_memory = TriState::kUnset;
    return;
  }

  options->crashpad_handler_behavior =
      CrashpadInfoClientOptions::TriStateFromCrashpadInfo(
          crashpad_info.crashpad_handler_behavior);

  options->system_crash_reporter_forwarding =
      CrashpadInfoClientOptions::TriStateFromCrashpadInfo(
          crashpad_info.system_crash_reporter_forwarding);

  options->gather_indirectly_referenced_memory =
      CrashpadInfoClientOptions::TriStateFromCrashpadInfo(
          crashpad_info.gather_indirectly_referenced_memory);

  options->indirectly_referenced_memory_cap =
      crashpad_info.indirectly_referenced_memory_cap;
}

std::string ModuleSnapshotFuchsia::Name() const {
  return name_;
}

uint64_t ModuleSnapshotFuchsia::Address() const {
  return base_;
}

uint64_t ModuleSnapshotFuchsia::Size() const {
  return size_;
}

time_t ModuleSnapshotFuchsia::Timestamp() const {
  NOTREACHED();
  return 0;
}

void ModuleSnapshotFuchsia::FileVersion(uint16_t* version_0,
                                        uint16_t* version_1,
                                        uint16_t* version_2,
                                        uint16_t* version_3) const {}

void ModuleSnapshotFuchsia::SourceVersion(uint16_t* version_0,
                                          uint16_t* version_1,
                                          uint16_t* version_2,
                                          uint16_t* version_3) const {}

ModuleSnapshot::ModuleType ModuleSnapshotFuchsia::GetModuleType() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return kModuleTypeUnknown;
}


void ModuleSnapshotFuchsia::UUIDAndAge(crashpad::UUID* uuid,
                                       uint32_t* age) const {}

std::string ModuleSnapshotFuchsia::DebugFileName() const {
  return buildid_;
}

std::vector<std::string> ModuleSnapshotFuchsia::AnnotationsVector() const {
  NOTREACHED();
  return std::vector<std::string>();
}

std::map<std::string, std::string> ModuleSnapshotFuchsia::AnnotationsSimpleMap()
    const {
  NOTREACHED();
  return std::map<std::string, std::string>();
}

std::vector<AnnotationSnapshot> ModuleSnapshotFuchsia::AnnotationObjects()
    const {
  NOTREACHED();
  return std::vector<AnnotationSnapshot>();
}

std::set<CheckedRange<uint64_t>> ModuleSnapshotFuchsia::ExtraMemoryRanges()
    const {
  NOTREACHED();
  return std::set<CheckedRange<uint64_t>>();
}

std::vector<const UserMinidumpStream*>
ModuleSnapshotFuchsia::CustomMinidumpStreams() const {
  NOTREACHED();
  return std::vector<const UserMinidumpStream*>();
}

}  // namespace internal
}  // namespace crashpad
