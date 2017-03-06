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

#include "snapshot/win/pe_image_annotations_reader.h"

#include <string.h>
#include <sys/types.h>

#include "base/strings/utf_string_conversions.h"
#include "client/annotations.h"
#include "client/simple_string_dictionary.h"
#include "snapshot/win/pe_image_reader.h"
#include "snapshot/win/process_reader_win.h"
#include "util/stdlib/map_insert.h"
#include "util/win/process_structs.h"

namespace crashpad {

namespace {

std::string ReadNulTerminatedString(ProcessReaderWin* process_reader,
                                    WinVMAddress start_at) {
  std::string into;
  into.resize(8192);
  // TODO(scottmg): Not sure how much we should read or search here. It seems
  // like this is probably more than necessary, and we probably don't want to
  // search forever to try to find a NUL anyway. On the other hand, it could be
  // annoying to lose the end of a value if this is too small.
  WinVMSize bytes_read = process_reader->ReadAvailableMemory(
      start_at, into.size() * sizeof(into[0]), &into[0]);
  into.resize(static_cast<unsigned int>(bytes_read / sizeof(into[0])));
  size_t at = into.find('\0');
  if (at != std::string::npos)
    into.resize(at);
  return into;
}

}  // namespace

PEImageAnnotationsReader::PEImageAnnotationsReader(
    ProcessReaderWin* process_reader,
    const PEImageReader* pe_image_reader,
    const std::wstring& name)
    : name_(name),
      process_reader_(process_reader),
      pe_image_reader_(pe_image_reader) {
}

std::map<std::string, std::string> PEImageAnnotationsReader::SimpleMap() const {
  std::map<std::string, std::string> annotations;
  if (process_reader_->Is64Bit()) {
    ReadCrashpadSimpleAnnotations<process_types::internal::Traits64>(
        &annotations);
  } else {
    ReadCrashpadSimpleAnnotations<process_types::internal::Traits32>(
        &annotations);
  }

  // Read the other style of annotations too (CRASHPAD_DEFINE_ANNOTATION_*).
  if (process_reader_->Is64Bit()) {
    ReadRawAnnotationsIntoMap<process_types::internal::Traits64>(&annotations);
  } else {
    ReadRawAnnotationsIntoMap<process_types::internal::Traits32>(&annotations);
  }

  return annotations;
}

template <class Traits>
void PEImageAnnotationsReader::ReadCrashpadSimpleAnnotations(
    std::map<std::string, std::string>* simple_map_annotations) const {
  process_types::CrashpadInfo<Traits> crashpad_info;
  if (!pe_image_reader_->GetCrashpadInfo(&crashpad_info))
    return;

  if (!crashpad_info.simple_annotations)
    return;

  std::vector<SimpleStringDictionary::Entry>
      simple_annotations(SimpleStringDictionary::num_entries);
  if (!process_reader_->ReadMemory(
          crashpad_info.simple_annotations,
          simple_annotations.size() * sizeof(simple_annotations[0]),
          &simple_annotations[0])) {
    LOG(WARNING) << "could not read simple annotations from "
                 << base::UTF16ToUTF8(name_);
    return;
  }

  for (const auto& entry : simple_annotations) {
    size_t key_length = strnlen(entry.key, sizeof(entry.key));
    if (key_length) {
      std::string key(entry.key, key_length);
      std::string value(entry.value, strnlen(entry.value, sizeof(entry.value)));
      if (!simple_map_annotations->insert(std::make_pair(key, value)).second) {
        LOG(INFO) << "duplicate simple annotation " << key << " in "
                  << base::UTF16ToUTF8(name_);
      }
    }
  }
}

template <class Traits>
void PEImageAnnotationsReader::ReadRawAnnotationsIntoMap(
    std::map<std::string, std::string>* map) const {
  std::vector<process_types::RawAnnotation<Traits>> annotations;
  if (!pe_image_reader_->GetCrashpadRawAnnotations<Traits>(&annotations)) {
    return;
  }

  for (const auto& annotation : annotations) {
    if (annotation.key_name == 0)
      continue;

    std::string key =
        ReadNulTerminatedString(process_reader_, annotation.key_name);

    if (annotation.type !=
            static_cast<uint32_t>(
                crashpad::internal::RawAnnotation::Type::kString) &&
        annotation.type !=
            static_cast<uint32_t>(
                crashpad::internal::RawAnnotation::Type::kStringOwned)) {
      LOG(ERROR) << "unexpected annotation type " << annotation.type;
      continue;
    }

    std::string value =
        ReadNulTerminatedString(process_reader_, annotation.data);

    MapInsertOrReplace(map, key, value, nullptr);
  }
}

}  // namespace crashpad
