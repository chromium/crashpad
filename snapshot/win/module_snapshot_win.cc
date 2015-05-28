// Copyright 2015 The Crashpad Authors. All rights reserved.
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

#include "snapshot/win/module_snapshot_win.h"

#include "base/strings/utf_string_conversions.h"
#include "snapshot/win/pe_image_annotations_reader.h"
#include "snapshot/win/pe_image_reader.h"
#include "util/misc/tri_state.h"
#include "util/misc/uuid.h"
#include "util/win/module_version.h"

namespace crashpad {
namespace internal {

ModuleSnapshotWin::ModuleSnapshotWin()
    : ModuleSnapshot(),
      name_(),
      timestamp_(0),
      process_reader_(nullptr),
      initialized_() {
}

ModuleSnapshotWin::~ModuleSnapshotWin() {
}

bool ModuleSnapshotWin::Initialize(
    ProcessReaderWin* process_reader,
    const ProcessInfo::Module& process_reader_module) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  process_reader_ = process_reader;
  name_ = process_reader_module.name;
  timestamp_ = process_reader_module.timestamp;
  pe_image_reader_.reset(new PEImageReader());
  if (!pe_image_reader_->Initialize(process_reader_,
                                    process_reader_module.dll_base,
                                    process_reader_module.size,
                                    base::UTF16ToUTF8(name_))) {
    return false;
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

void ModuleSnapshotWin::GetCrashpadOptions(CrashpadInfoClientOptions* options) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  process_types::CrashpadInfo crashpad_info;
  if (!pe_image_reader_->GetCrashpadInfo(&crashpad_info)) {
    options->crashpad_handler_behavior = TriState::kUnset;
    options->system_crash_reporter_forwarding = TriState::kUnset;
    return;
  }

  options->crashpad_handler_behavior =
      CrashpadInfoClientOptions::TriStateFromCrashpadInfo(
          crashpad_info.crashpad_handler_behavior);

  options->system_crash_reporter_forwarding =
      CrashpadInfoClientOptions::TriStateFromCrashpadInfo(
          crashpad_info.system_crash_reporter_forwarding);
}

std::string ModuleSnapshotWin::Name() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return base::UTF16ToUTF8(name_);
}

uint64_t ModuleSnapshotWin::Address() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return pe_image_reader_->Address();
}

uint64_t ModuleSnapshotWin::Size() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return pe_image_reader_->Size();
}

time_t ModuleSnapshotWin::Timestamp() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return timestamp_;
}

void ModuleSnapshotWin::FileVersion(uint16_t* version_0,
                                    uint16_t* version_1,
                                    uint16_t* version_2,
                                    uint16_t* version_3) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  VS_FIXEDFILEINFO ffi;
  if (GetModuleVersionAndType(base::FilePath(name_), &ffi)) {
    *version_0 = ffi.dwFileVersionMS >> 16;
    *version_1 = ffi.dwFileVersionMS & 0xffff;
    *version_2 = ffi.dwFileVersionLS >> 16;
    *version_3 = ffi.dwFileVersionLS & 0xffff;
  } else {
    *version_0 = 0;
    *version_1 = 0;
    *version_2 = 0;
    *version_3 = 0;
  }
}

void ModuleSnapshotWin::SourceVersion(uint16_t* version_0,
                                      uint16_t* version_1,
                                      uint16_t* version_2,
                                      uint16_t* version_3) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  VS_FIXEDFILEINFO ffi;
  if (GetModuleVersionAndType(base::FilePath(name_), &ffi)) {
    *version_0 = ffi.dwProductVersionMS >> 16;
    *version_1 = ffi.dwProductVersionMS & 0xffff;
    *version_2 = ffi.dwProductVersionLS >> 16;
    *version_3 = ffi.dwProductVersionLS & 0xffff;
  } else {
    *version_0 = 0;
    *version_1 = 0;
    *version_2 = 0;
    *version_3 = 0;
  }
}

ModuleSnapshot::ModuleType ModuleSnapshotWin::GetModuleType() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  VS_FIXEDFILEINFO ffi;
  if (GetModuleVersionAndType(base::FilePath(name_), &ffi)) {
    if (ffi.dwFileType == VFT_APP)
      return ModuleSnapshot::kModuleTypeExecutable;
    if (ffi.dwFileType == VFT_DLL)
      return ModuleSnapshot::kModuleTypeSharedLibrary;
    if (ffi.dwFileType == VFT_DRV || ffi.dwFileType == VFT_VXD)
      return ModuleSnapshot::kModuleTypeLoadableModule;
  }
  return ModuleSnapshot::kModuleTypeUnknown;
}

void ModuleSnapshotWin::UUID(crashpad::UUID* uuid) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  CHECK(false) << "TODO(scottmg)";
}

std::vector<std::string> ModuleSnapshotWin::AnnotationsVector() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  // These correspond to system-logged things on Mac. We don't currently track
  // any of these on Windows, but could in the future.
  // See https://code.google.com/p/crashpad/issues/detail?id=38.
  return std::vector<std::string>();
}

std::map<std::string, std::string> ModuleSnapshotWin::AnnotationsSimpleMap()
    const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  PEImageAnnotationsReader annotations_reader(
      process_reader_, pe_image_reader_.get(), name_);
  return annotations_reader.SimpleMap();
}

}  // namespace internal
}  // namespace crashpad
