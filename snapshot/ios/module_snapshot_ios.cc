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
#include "util/misc/from_pointer_cast.h"
#include "util/misc/tri_state.h"
#include "util/misc/uuid.h"
#include "util/stdlib/strnlen.h"

namespace crashpad {
namespace internal {

template <typename T>
class ValidateMemory {
 private:
  T* pointer_data_;
 public:
  ValidateMemory(T* data) : pointer_data_(nullptr) {
    const int byte_count = sizeof(T);
    vm_offset_t region;
    mach_msg_type_number_t region_count;
    kern_return_t result = vm_read(mach_task_self(),
                                   (vm_address_t)data,
                                   (vm_size_t)byte_count,
                                   &region,
                                   &region_count);
    if (result != KERN_SUCCESS) {
      pointer_data_ = 0;
    } else if (region_count != byte_count) {
      pointer_data_ = 0;
      vm_deallocate(mach_task_self(), region, region_count);
    } else {
      pointer_data_ = (T*)region;
    }
  }
  ~ValidateMemory() {
    if (pointer_data_) {
      vm_deallocate(mach_task_self(), (vm_address_t)pointer_data_, sizeof(T));
    }
  }

  T& operator*() { return *pointer_data_; }

  T* operator->() { return pointer_data_; }
};

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
//  const struct mach_header_64* header = (mach_header_64*)address_;
  ::crashpad::internal::ValidateMemory<const struct mach_header_64> header((mach_header_64*)address_);
  ::crashpad::internal::ValidateMemory<struct load_command> command((struct load_command*)&header + 1); // + 1
//  struct load_command* command = (struct load_command*)(header + 1);
  // header and command may be trash
//  if (!ValidateMemory(header, sizeof(mach_header_64)) ||
//      !ValidateMemory(command, sizeof(load_command)))
//    return false;
  for (uint32_t i = 0; i < header->ncmds; i++) {
    if (command->cmd == LC_SEGMENT_64) {
      struct segment_command_64* segment =
          (struct segment_command_64*)(&command);
//      if (!ValidateMemory(segment, sizeof(segment_command_64)))
//        return false;
      if (strcmp(segment->segname, "__TEXT") == 0) {
        size_ = segment->vmsize;
      }
    }
    if (command->cmd == LC_ID_DYLIB) {
      struct dylib_command* dylib = (struct dylib_command*)(&command);
//      if (!ValidateMemory(dylib, sizeof(dylib_command)))
//        return false;
      dylib_version_ = dylib->dylib.current_version;
    }
    if (command->cmd == LC_SOURCE_VERSION) {
      struct source_version_command* source_version =
          (struct source_version_command*)(&command);
//      if (!ValidateMemory(source_version, sizeof(source_version_command)))
//        return false;
      source_version_ = source_version->version;
    }
    if (command->cmd == LC_UUID) {
      struct uuid_command* uuid = (struct uuid_command*)(&command);
//      if (!ValidateMemory(uuid, sizeof(uuid_command)))
//        return false;
      memcpy(uuid_, uuid->uuid, sizeof(uuid->uuid));
    }

    command = ::crashpad::internal::ValidateMemory<struct load_command>((load_command*)((uint8_t*)&command + &command->cmdsize));
//    if (!ValidateMemory(command, sizeof(load_command)))
//      return false;
  }

  filetype_ = header->filetype;

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

bool ModuleSnapshotIOS::ValidateMemory(const void* const memory,
                                       const int byte_count) {
  static char test_buffer[1024];
  vm_address_t vm_memory = reinterpret_cast<vm_address_t>(memory);
  int bytes_remaining = byte_count;
  while (bytes_remaining > 0) {
    int bytes_to_copy = bytes_remaining > 1024 ? 1024 : bytes_remaining;
    vm_size_t bytes_copied = 0;
    kern_return_t result = vm_read_overwrite(mach_task_self(),
                                             vm_memory,
                                             (vm_size_t)bytes_to_copy,
                                             (vm_address_t)test_buffer,
                                             &bytes_copied);
    if (result != KERN_SUCCESS) {
      return false;
    }
    if ((int)bytes_copied != bytes_to_copy) {
      return false;
    }
    bytes_remaining -= bytes_to_copy;
    vm_memory = vm_memory + bytes_to_copy;
  }
  return bytes_remaining == 0;
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
