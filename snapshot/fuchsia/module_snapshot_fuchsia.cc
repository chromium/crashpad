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

#include "base/logging.h"
#include "client/crashpad_info.h"
#include "snapshot/crashpad_types/image_annotation_reader.h"

namespace crashpad {
namespace internal {

ModuleSnapshotFuchsia::ModuleSnapshotFuchsia() = default;

ModuleSnapshotFuchsia::~ModuleSnapshotFuchsia() = default;

bool ModuleSnapshotFuchsia::Initialize(
    ProcessReader* process_reader,
    const ProcessReader::Module& process_reader_module) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  process_reader_ = process_reader;
  name_ = process_reader_module.name;
  timestamp_ = 0;  // TODO(scottmg): Anything available?
  elf_image_reader_ = process_reader_module.reader;
  if (!elf_image_reader_) {
    return false;
  }

  // XXX XXX XXX
  // TODO(scottmg): Testing to see if the note makes it into the final binary.
  // XXX XXX XXX
  std::unique_ptr<ElfImageReader::NoteReader> notes =
      elf_image_reader_->NotesWithNameAndType("CrashpadInfo", 1, -1);
  std::string desc;
  if (notes->NextNote(nullptr, nullptr, &desc) ==
      ElfImageReader::NoteReader::Result::kSuccess) {
    uintptr_t p = *reinterpret_cast<uintptr_t*>(&desc[0]);
    LOG(ERROR) << "desc=0x" << std::hex << p;
  }

#if 0
  VMAddress info_address;
  VMSize info_size;
  if (elf_image_reader_->GetDynamicSymbol(
          "g_crashpad_info", &info_address, &info_size)) {
    ProcessMemoryRange range;
    if (range.Initialize(*elf_image_reader_->Memory()) &&
        range.RestrictRange(info_address, info_size)) {
      auto info = std::make_unique<CrashpadInfoReader>();
      if (info->Initialize(&range, info_address)) {
        crashpad_info_ = std::move(info);
      }
    }
  }
#endif

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

bool ModuleSnapshotFuchsia::GetCrashpadOptions(
    CrashpadInfoClientOptions* options) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  if (!crashpad_info_) {
    LOG(ERROR) << "no crashpad_info";
    return false;
  }

  options->crashpad_handler_behavior =
      crashpad_info_->CrashpadHandlerBehavior();
  options->system_crash_reporter_forwarding =
      crashpad_info_->SystemCrashReporterForwarding();
  options->gather_indirectly_referenced_memory =
      crashpad_info_->GatherIndirectlyReferencedMemory();
  options->indirectly_referenced_memory_cap =
      crashpad_info_->IndirectlyReferencedMemoryCap();
  return true;
}

std::string ModuleSnapshotFuchsia::Name() const {
  return name_;
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
                                        uint16_t* version_3) const {
  NOTREACHED();
}

void ModuleSnapshotFuchsia::SourceVersion(uint16_t* version_0,
                                          uint16_t* version_1,
                                          uint16_t* version_2,
                                          uint16_t* version_3) const {
  NOTREACHED();
}

ModuleSnapshot::ModuleType ModuleSnapshotFuchsia::GetModuleType() const {
  NOTREACHED();
  return kModuleTypeUnknown;
}

void ModuleSnapshotFuchsia::UUIDAndAge(crashpad::UUID* uuid,
                                       uint32_t* age) const {
  NOTREACHED();
}

std::string ModuleSnapshotFuchsia::DebugFileName() const {
  NOTREACHED();
  return std::string();
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
