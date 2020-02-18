// Copyright 2020 The Crashpad Authors. All rights reserved.
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

#include "snapshot/ios/module_snapshot_ios.h"

#include <mach-o/loader.h>
#include <mach/mach.h>

#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "snapshot/mac/mach_o_image_annotations_reader.h"
#include "snapshot/mac/mach_o_image_reader.h"
#include "util/misc/tri_state.h"
#include "util/misc/uuid.h"
#include "util/stdlib/strnlen.h"

namespace crashpad {
namespace internal {

ModuleSnapshotIOS::ModuleSnapshotIOS()
    : ModuleSnapshot(),
      name_(),
      timestamp_(0),
      process_reader_(nullptr),
      initialized_() {}

ModuleSnapshotIOS::~ModuleSnapshotIOS() {}

bool ModuleSnapshotIOS::Initialize(
    ProcessReaderIOS* process_reader,
    const ProcessReaderIOS::Module& process_reader_module) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  process_reader_ = process_reader;
  name_ = process_reader_module.name;
  timestamp_ = process_reader_module.timestamp;

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

std::string ModuleSnapshotIOS::Name() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return name_;
}

uint64_t ModuleSnapshotIOS::Address() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return 0;
}

uint64_t ModuleSnapshotIOS::Size() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return 0;
}

time_t ModuleSnapshotIOS::Timestamp() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return timestamp_;
}

void ModuleSnapshotIOS::FileVersion(uint16_t* version_0,
                                    uint16_t* version_1,
                                    uint16_t* version_2,
                                    uint16_t* version_3) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
}

void ModuleSnapshotIOS::SourceVersion(uint16_t* version_0,
                                      uint16_t* version_1,
                                      uint16_t* version_2,
                                      uint16_t* version_3) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
}

ModuleSnapshot::ModuleType ModuleSnapshotIOS::GetModuleType() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return ModuleType();
}

void ModuleSnapshotIOS::UUIDAndAge(crashpad::UUID* uuid, uint32_t* age) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *age = 0;
}

std::string ModuleSnapshotIOS::DebugFileName() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return base::FilePath(Name()).BaseName().value();
}

std::vector<uint8_t> ModuleSnapshotIOS::BuildID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<uint8_t>();
}

std::vector<std::string> ModuleSnapshotIOS::AnnotationsVector() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<std::string>();
}

std::map<std::string, std::string> ModuleSnapshotIOS::AnnotationsSimpleMap()
    const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::map<std::string, std::string>();
}

std::vector<AnnotationSnapshot> ModuleSnapshotIOS::AnnotationObjects() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<AnnotationSnapshot>();
}

std::set<CheckedRange<uint64_t>> ModuleSnapshotIOS::ExtraMemoryRanges() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::set<CheckedRange<uint64_t>>();
}

std::vector<const UserMinidumpStream*>
ModuleSnapshotIOS::CustomMinidumpStreams() const {
  return std::vector<const UserMinidumpStream*>();
}

}  // namespace internal
}  // namespace crashpad
