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
#include "client/simple_string_dictionary.h"
#include "snapshot/win/pe_image_reader.h"
#include "snapshot/win/process_reader_win.h"
#include "util/win/process_structs.h"

namespace crashpad {

PEImageAnnotationsReader::PEImageAnnotationsReader(
    ProcessReaderWin* process_reader,
    const PEImageReader* pe_image_reader,
    const std::wstring& name)
    : name_(name),
      process_reader_(process_reader),
      pe_image_reader_(pe_image_reader) {
}

std::map<std::string, std::string> PEImageAnnotationsReader::SimpleMap() const {
  std::map<std::string, std::string> simple_map_annotations;
  if (process_reader_->Is64Bit()) {
    ReadCrashpadSimpleAnnotations<process_types::internal::Traits64>(
        &simple_map_annotations);
  } else {
    ReadCrashpadSimpleAnnotations<process_types::internal::Traits32>(
        &simple_map_annotations);
  }
  return simple_map_annotations;
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

}  // namespace crashpad
