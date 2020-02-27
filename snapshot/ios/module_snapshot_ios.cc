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
#include "util/misc/tri_state.h"
#include "util/misc/uuid.h"
#include "util/stdlib/strnlen.h"

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

// static.
mach_vm_address_t ModuleSnapshotIOS::DyldAllImageInfo(
    mach_vm_size_t* all_image_info_size) {
  task_dyld_info_data_t dyld_info;
  mach_msg_type_number_t count = TASK_DYLD_INFO_COUNT;

  kern_return_t kr = task_info(mach_task_self(),
                               TASK_DYLD_INFO,
                               reinterpret_cast<task_info_t>(&dyld_info),
                               &count);
  if (kr != KERN_SUCCESS) {
    MACH_LOG(WARNING, kr) << "task_info";
    return 0;
  }

  if (all_image_info_size) {
    *all_image_info_size = dyld_info.all_image_info_size;
  }
  return dyld_info.all_image_info_addr;
}

bool ModuleSnapshotIOS::InitializeDyld(const dyld_all_image_infos* image) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  name_ = image->dyldPath;
  address_ = FromPointerCast<uint64_t>(image->dyldImageLoadAddress);
  return FinishInitialization();
}

bool ModuleSnapshotIOS::Initialize(const dyld_image_info* image) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  name_ = image->imageFilePath;
  address_ = FromPointerCast<uint64_t>(image->imageLoadAddress);
  timestamp_ = image->imageFileModDate;
  return FinishInitialization();
}

bool ModuleSnapshotIOS::FinishInitialization() {
  // Size, FileVersion and SourceVersion, UUID
  const struct mach_header_64* header = (mach_header_64*)address_;
  struct load_command* command = (struct load_command*)(header + 1);
  for (uint32_t i = 0; i < header->ncmds;
       i++, command = (load_command*)((uint8_t*)command + command->cmdsize)) {
    if (command->cmd == LC_SEGMENT_64) {
      struct segment_command_64* segment =
          (struct segment_command_64*)(&command);
      if (strcmp(segment->segname, "__TEXT") == 0) {
        size_ = segment->vmsize;
      }
    }
    if (command->cmd == LC_ID_DYLIB) {
      struct dylib_command* dylib = (struct dylib_command*)(&command);
      dylib_version_ = dylib->dylib.current_version;
    }
    if (command->cmd == LC_SOURCE_VERSION) {
      struct source_version_command* source_version =
          (struct source_version_command*)(&command);
      source_version_ = source_version->version;
    }
    if (command->cmd == LC_UUID) {
      struct uuid_command* uuid = (struct uuid_command*)(&command);
      memcpy(uuid_, uuid->uuid, sizeof(uuid->uuid));
    }
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
  uuid->InitializeFromBytes(uuid_);
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
