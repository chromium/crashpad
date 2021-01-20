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

#include "snapshot/ios/module_snapshot_ios_intermediatedump.h"

#include <mach-o/loader.h>
#include <mach/mach.h>

#include "base/files/file_path.h"
#include "base/mac/mach_logging.h"
#include "client/annotation.h"
#include "util/ios/ios_intermediatedump_data.h"
#include "util/ios/ios_intermediatedump_list.h"
#include "util/misc/from_pointer_cast.h"
#include "util/misc/uuid.h"

namespace crashpad {
namespace internal {

ModuleSnapshotIOSIntermediatedump::ModuleSnapshotIOSIntermediatedump()
    : ModuleSnapshot(),
      name_(),
      address_(0),
      size_(0),
      timestamp_(0),
      dylib_version_(0),
      source_version_(0),
      filetype_(0),
      initialized_() {}

ModuleSnapshotIOSIntermediatedump::~ModuleSnapshotIOSIntermediatedump() {}

bool ModuleSnapshotIOSIntermediatedump::Initialize(
    const IOSIntermediatedumpMap& image_data) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  image_data[IntermediateDumpKey::kName].GetString(&name_);
  image_data[IntermediateDumpKey::kAddress].AsData().GetData<uint64_t>(
      &address_);
  image_data[IntermediateDumpKey::kSize].AsData().GetData<uint64_t>(&size_);
  image_data[IntermediateDumpKey::kTimestamp].AsData().GetData<time_t>(
      &timestamp_);
  image_data[IntermediateDumpKey::kDylibCurrentVersion]
      .AsData()
      .GetData<uint32_t>(&dylib_version_);
  image_data[IntermediateDumpKey::kSourceVersion].AsData().GetData<uint64_t>(
      &source_version_);
  image_data[IntermediateDumpKey::kFileType].AsData().GetData<uint32_t>(
      &filetype_);
  unsigned char const* bytes =
      image_data[IntermediateDumpKey::kUUID].AsData().data();
  if (!bytes ||
      image_data[IntermediateDumpKey::kUUID].AsData().length() != 16) {
    LOG(ERROR) << "Invalid module uuid.";
  } else {
    uuid_.InitializeFromBytes(bytes);
  }

  const auto& annotation_list =
      image_data[IntermediateDumpKey::kAnnotationsList].AsList();
  for (auto& annotation : annotation_list) {
    std::string name;
    const IOSIntermediatedumpMap& annotation_map = (*annotation).AsMap();
    annotation_map[IntermediateDumpKey::kAnnotationName].GetString(&name);
    if (name.empty() || name.length() > Annotation::kNameMaxLength) {
      LOG(ERROR) << "Invalid annotation name length.";
      continue;
    }

    uint16_t type;
    annotation_map[IntermediateDumpKey::kAnnotationType]
        .AsData()
        .GetData<uint16_t>(&type);

    const uint8_t* value =
        annotation_map[IntermediateDumpKey::kAnnotationValue].AsData().data();
    uint64_t length =
        annotation_map[IntermediateDumpKey::kAnnotationValue].AsData().length();

    if (!value || length > Annotation::kValueMaxSize) {
      LOG(ERROR) << "Invalid annotation value length.";
      continue;
    }

    std::vector<uint8_t> vector(value, value + length);
    annotation_objects_.push_back(AnnotationSnapshot(name, type, vector));
  }

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

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

std::string ModuleSnapshotIOSIntermediatedump::Name() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return name_;
}

uint64_t ModuleSnapshotIOSIntermediatedump::Address() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return address_;
}

uint64_t ModuleSnapshotIOSIntermediatedump::Size() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return size_;
}

time_t ModuleSnapshotIOSIntermediatedump::Timestamp() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return timestamp_;
}

void ModuleSnapshotIOSIntermediatedump::FileVersion(uint16_t* version_0,
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

void ModuleSnapshotIOSIntermediatedump::SourceVersion(
    uint16_t* version_0,
    uint16_t* version_1,
    uint16_t* version_2,
    uint16_t* version_3) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *version_0 = (source_version_ & 0xffff000000000000u) >> 48;
  *version_1 = (source_version_ & 0x0000ffff00000000u) >> 32;
  *version_2 = (source_version_ & 0x00000000ffff0000u) >> 16;
  *version_3 = source_version_ & 0x000000000000ffffu;
}

ModuleSnapshot::ModuleType ModuleSnapshotIOSIntermediatedump::GetModuleType()
    const {
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

void ModuleSnapshotIOSIntermediatedump::UUIDAndAge(crashpad::UUID* uuid,
                                                   uint32_t* age) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *uuid = uuid_;
  *age = 0;
}

std::string ModuleSnapshotIOSIntermediatedump::DebugFileName() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return base::FilePath(Name()).BaseName().value();
}

std::vector<uint8_t> ModuleSnapshotIOSIntermediatedump::BuildID() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<uint8_t>();
}

std::vector<std::string> ModuleSnapshotIOSIntermediatedump::AnnotationsVector()
    const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<std::string>();
}

std::map<std::string, std::string>
ModuleSnapshotIOSIntermediatedump::AnnotationsSimpleMap() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::map<std::string, std::string>();
}

std::vector<AnnotationSnapshot>
ModuleSnapshotIOSIntermediatedump::AnnotationObjects() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return annotation_objects_;
}

std::set<CheckedRange<uint64_t>>
ModuleSnapshotIOSIntermediatedump::ExtraMemoryRanges() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::set<CheckedRange<uint64_t>>();
}

std::vector<const UserMinidumpStream*>
ModuleSnapshotIOSIntermediatedump::CustomMinidumpStreams() const {
  return std::vector<const UserMinidumpStream*>();
}

}  // namespace internal
}  // namespace crashpad
