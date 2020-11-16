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
#include "base/mac/mach_logging.h"
#include "util/misc/from_pointer_cast.h"
#include "util/misc/uuid.h"

namespace crashpad {
namespace internal {

ModuleSnapshotIOS::ModuleSnapshotIOS()
    : ModuleSnapshot(),
      name_(),
      address_(0),
      size_(0),
      timestamp_(0),
      dylib_version_(0),
      source_version_(0),
      filetype_(0),
      initialized_() {}

ModuleSnapshotIOS::~ModuleSnapshotIOS() {}

bool ModuleSnapshotIOS::Initialize(const PackedMap& image_data) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  image_data["name"].GetString(&name_);
  image_data["address"].AsData().GetData<uint64_t>(&address_);
  image_data["size"].AsData().GetData<uint64_t>(&size_);
  image_data["timestamp"].AsData().GetData<time_t>(&timestamp_);
  image_data["dylib_current_version"].AsData().GetData<uint32_t>(
      &dylib_version_);
  image_data["source_version"].AsData().GetData<uint64_t>(&source_version_);
  image_data["filetype"].AsData().GetData<uint32_t>(&filetype_);
  uuid_.InitializeFromBytes(image_data["uuid"].AsData().data());
  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

bool ModuleSnapshotIOS::FinishInitialization() {
#ifndef ARCH_CPU_64_BITS
#error Only 64-bit Mach-O is supported
#endif
  DCHECK(address_);
  const mach_header_64* header =
      reinterpret_cast<const mach_header_64*>(address_);
  const load_command* command =
      reinterpret_cast<const load_command*>(header + 1);
  // Make sure that the basic load command structure doesn’t overflow the
  // space allotted for load commands, as well as iterating through ncmds.
  for (uint32_t cmd_index = 0, cumulative_cmd_size = 0;
       cmd_index <= header->ncmds && cumulative_cmd_size < header->sizeofcmds;
       ++cmd_index, cumulative_cmd_size += command->cmdsize) {
    if (command->cmd == LC_SEGMENT_64) {
      const segment_command_64* segment =
          reinterpret_cast<const segment_command_64*>(command);
      if (strcmp(segment->segname, SEG_TEXT) == 0) {
        size_ = segment->vmsize;
      }
    } else if (command->cmd == LC_ID_DYLIB) {
      const dylib_command* dylib =
          reinterpret_cast<const dylib_command*>(command);
      dylib_version_ = dylib->dylib.current_version;
    } else if (command->cmd == LC_SOURCE_VERSION) {
      const source_version_command* source_version =
          reinterpret_cast<const source_version_command*>(command);
      source_version_ = source_version->version;
    } else if (command->cmd == LC_UUID) {
      const uuid_command* uuid = reinterpret_cast<const uuid_command*>(command);
      uuid_.InitializeFromBytes(uuid->uuid);
    }

    command = reinterpret_cast<const load_command*>(
        reinterpret_cast<const uint8_t*>(command) + command->cmdsize);

    // TODO(justincohen): Warn-able things:
    // - Bad Mach-O magic (and give up trying to process the module)
    // - Unrecognized Mach-O type
    // - No SEG_TEXT
    // - More than one SEG_TEXT
    // - More than one LC_ID_DYLIB, LC_SOURCE_VERSION, or LC_UUID
    // - No LC_ID_DYLIB in a dylib file
    // - LC_ID_DYLIB in a non-dylib file
    // And more optional:
    // - Missing LC_UUID (although it leaves us with a big "?")
    // - Missing LC_SOURCE_VERSION.
  }

  filetype_ = header->filetype;

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

std::string ModuleSnapshotIOS::Name() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return name_;
}

uint64_t ModuleSnapshotIOS::Address() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return address_;
}

uint64_t ModuleSnapshotIOS::Size() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return size_;
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
  if (filetype_ == MH_DYLIB) {
    *version_0 = (dylib_version_ & 0xffff0000) >> 16;
    *version_1 = (dylib_version_ & 0x0000ff00) >> 8;
    *version_2 = (dylib_version_ & 0x000000ff);
    *version_3 = 0;
  } else {
    *version_0 = 0;
    *version_1 = 0;
    *version_2 = 0;
    *version_3 = 0;
  }
}

void ModuleSnapshotIOS::SourceVersion(uint16_t* version_0,
                                      uint16_t* version_1,
                                      uint16_t* version_2,
                                      uint16_t* version_3) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *version_0 = (source_version_ & 0xffff000000000000u) >> 48;
  *version_1 = (source_version_ & 0x0000ffff00000000u) >> 32;
  *version_2 = (source_version_ & 0x00000000ffff0000u) >> 16;
  *version_3 = source_version_ & 0x000000000000ffffu;
}

ModuleSnapshot::ModuleType ModuleSnapshotIOS::GetModuleType() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  switch (filetype_) {
    case MH_EXECUTE:
      return kModuleTypeExecutable;
    case MH_DYLIB:
      return kModuleTypeSharedLibrary;
    case MH_DYLINKER:
      return kModuleTypeDynamicLoader;
    case MH_BUNDLE:
      return kModuleTypeLoadableModule;
    default:
      return kModuleTypeUnknown;
  }
}

void ModuleSnapshotIOS::UUIDAndAge(crashpad::UUID* uuid, uint32_t* age) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *uuid = uuid_;
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
