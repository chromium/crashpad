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

#include "snapshot/linux/module_snapshot_linux.h"

#include <algorithm>

#include "base/files/file_path.h"
#include "snapshot/crashpad_types/image_annotation_reader.h"
#include "util/misc/elf_note_types.h"

namespace crashpad {
namespace internal {

ModuleSnapshotLinux::ModuleSnapshotLinux()
    : ModuleSnapshot(),
      name_(),
      elf_reader_(nullptr),
      crashpad_info_(),
      type_(kModuleTypeUnknown),
      initialized_() {}

ModuleSnapshotLinux::~ModuleSnapshotLinux() = default;

bool ModuleSnapshotLinux::Initialize(
    const ProcessReader::Module& process_reader_module) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  if (!process_reader_module.elf_reader) {
    LOG(ERROR) << "no elf reader";
    return false;
  }

  name_ = process_reader_module.name;
  elf_reader_ = process_reader_module.elf_reader;
  type_ = process_reader_module.type;

  // The data payload is only sizeof(VMAddress) in the note, but add a bit to
  // account for the name, header, and padding.
  constexpr ssize_t kMaxNoteSize = 256;
  std::unique_ptr<ElfImageReader::NoteReader> notes =
      elf_reader_->NotesWithNameAndType(CRASHPAD_ELF_NOTE_NAME,
                                        CRASHPAD_ELF_NOTE_TYPE_CRASHPAD_INFO,
                                        kMaxNoteSize);
  std::string desc;
  VMAddress info_address;
  if (notes->NextNote(nullptr, nullptr, &desc) ==
      ElfImageReader::NoteReader::Result::kSuccess) {
    info_address = *reinterpret_cast<VMAddress*>(&desc[0]);

    ProcessMemoryRange range;
    if (range.Initialize(*elf_reader_->Memory())) {
      auto info = std::make_unique<CrashpadInfoReader>();
      if (info->Initialize(&range, info_address)) {
        crashpad_info_ = std::move(info);
      }
    }
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

bool ModuleSnapshotLinux::GetCrashpadOptions(
    CrashpadInfoClientOptions* options) {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  if (!crashpad_info_) {
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

std::string ModuleSnapshotLinux::Name() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return name_;
}

uint64_t ModuleSnapshotLinux::Address() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return elf_reader_->Address();
}

uint64_t ModuleSnapshotLinux::Size() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return elf_reader_->Size();
}

time_t ModuleSnapshotLinux::Timestamp() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return 0;
}

void ModuleSnapshotLinux::FileVersion(uint16_t* version_0,
                                      uint16_t* version_1,
                                      uint16_t* version_2,
                                      uint16_t* version_3) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *version_0 = 0;
  *version_1 = 0;
  *version_2 = 0;
  *version_3 = 0;
}

void ModuleSnapshotLinux::SourceVersion(uint16_t* version_0,
                                        uint16_t* version_1,
                                        uint16_t* version_2,
                                        uint16_t* version_3) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *version_0 = 0;
  *version_1 = 0;
  *version_2 = 0;
  *version_3 = 0;
}

ModuleSnapshot::ModuleType ModuleSnapshotLinux::GetModuleType() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return type_;
}

void ModuleSnapshotLinux::UUIDAndAge(crashpad::UUID* uuid,
                                     uint32_t* age) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  *age = 0;

  std::unique_ptr<ElfImageReader::NoteReader> notes =
      elf_reader_->NotesWithNameAndType(ELF_NOTE_GNU, NT_GNU_BUILD_ID, 64);
  std::string desc;
  notes->NextNote(nullptr, nullptr, &desc);
  desc.insert(desc.end(), 16 - std::min(desc.size(), size_t{16}), '\0');
  uuid->InitializeFromBytes(reinterpret_cast<const uint8_t*>(&desc[0]));
}

std::string ModuleSnapshotLinux::DebugFileName() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return base::FilePath(Name()).BaseName().value();
}

std::vector<std::string> ModuleSnapshotLinux::AnnotationsVector() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::vector<std::string>();
}

std::map<std::string, std::string> ModuleSnapshotLinux::AnnotationsSimpleMap()
    const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  std::map<std::string, std::string> annotations;
  if (crashpad_info_ && crashpad_info_->SimpleAnnotations()) {
    ImageAnnotationReader reader(elf_reader_->Memory());
    reader.SimpleMap(crashpad_info_->SimpleAnnotations(), &annotations);
  }
  return annotations;
}

std::vector<AnnotationSnapshot> ModuleSnapshotLinux::AnnotationObjects() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  std::vector<AnnotationSnapshot> annotations;
  if (crashpad_info_ && crashpad_info_->AnnotationsList()) {
    ImageAnnotationReader reader(elf_reader_->Memory());
    reader.AnnotationsList(crashpad_info_->AnnotationsList(), &annotations);
  }
  return annotations;
}

std::set<CheckedRange<uint64_t>> ModuleSnapshotLinux::ExtraMemoryRanges()
    const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return std::set<CheckedRange<uint64_t>>();
}

std::vector<const UserMinidumpStream*>
ModuleSnapshotLinux::CustomMinidumpStreams() const {
  return std::vector<const UserMinidumpStream*>();
}

}  // namespace internal
}  // namespace crashpad
