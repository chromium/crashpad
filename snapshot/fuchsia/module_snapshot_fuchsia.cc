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

bool ModuleSnapshotFuchsia::Initialize(zx_handle_t process) {
  return true;
}

void ModuleSnapshotFuchsia::GetCrashpadOptions(
    CrashpadInfoClientOptions* options) {}

std::string ModuleSnapshotFuchsia::Name() const {
  NOTREACHED();
  return std::string();
}

uint64_t ModuleSnapshotFuchsia::Address() const {
  NOTREACHED();
  return 0;
}

uint64_t ModuleSnapshotFuchsia::Size() const {
  NOTREACHED();
  return 0;
}

time_t ModuleSnapshotFuchsia::Timestamp() const {
  return timestamp_;
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
  NOTREACHED();
  return 0;
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
