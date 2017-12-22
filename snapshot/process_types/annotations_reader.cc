// Copyright 2017 The Crashpad Authors. All rights reserved.
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

#include "snapshot/process_types/annotations_reader.h"

#include <string.h>
#include <sys/types.h>

#include "client/annotation.h"
#include "client/simple_string_dictionary.h"
#include "snapshot/snapshot_constants.h"
#include "util/misc/traits.h"

namespace crashpad {

namespace process_types {

template <class Traits>
struct Annotation {
  typename Traits::Pointer link_node;
  typename Traits::Pointer name;
  typename Traits::Pointer value;
  uint32_t size;
  uint16_t type;
};

template <class Traits>
struct AnnotationList {
  typename Traits::Pointer tail_pointer;
  Annotation<Traits> head;
  Annotation<Traits> tail;
};

namespace internal {
}

}  // namespace process_types

AnnotationsReader::AnnotationsReader(const ProcessMemoryRange* memory)
    : memory_(memory) {}

std::map<std::string, std::string> AnnotationsReader::SimpleMap(
    VMAddress address) const {
  std::map<std::string, std::string> simple_map_annotations;
  if (memory_->Is64Bit()) {
    ReadCrashpadSimpleAnnotations<Traits64>(
        address, &simple_map_annotations);
  } else {
    ReadCrashpadSimpleAnnotations<Traits32>(
        address, &simple_map_annotations);
  }
  return simple_map_annotations;
}

std::vector<AnnotationSnapshot> AnnotationsReader::AnnotationsList()
    const {
  std::vector<AnnotationSnapshot> annotations;
  if (memory_->Is64Bit()) {
    ReadCrashpadAnnotationsList<Traits64>(
        &annotations);
  } else {
    ReadCrashpadAnnotationsList<Traits32>(
        &annotations);
  }
  return annotations;
}

template <class Traits>
void AnnotationsReader::ReadCrashpadSimpleAnnotations(
    VMAddress address,
    std::map<std::string, std::string>* simple_map_annotations) const {
  std::vector<SimpleStringDictionary::Entry>
      simple_annotations(SimpleStringDictionary::num_entries);
  if (!memory_->Read(
          address,
          simple_annotations.size() * sizeof(simple_annotations[0]),
          &simple_annotations[0])) {
    LOG(WARNING) << "could not read simple annotations from " << name_;
    return;
  }

  for (const auto& entry : simple_annotations) {
    size_t key_length = strnlen(entry.key, sizeof(entry.key));
    if (key_length) {
      std::string key(entry.key, key_length);
      std::string value(entry.value, strnlen(entry.value, sizeof(entry.value)));
      if (!simple_map_annotations->insert(std::make_pair(key, value)).second) {
        LOG(INFO) << "duplicate simple annotation " << key << " in " << name_;
      }
    }
  }
}

template <class Traits>
void AnnotationsReader::ReadCrashpadAnnotationsList(
    VMAddress address,
    std::vector<AnnotationSnapshot>* vector_annotations) const {

  process_types::AnnotationList<Traits> annotation_list_object;
  if (!memory_->Read(address,
                                   sizeof(annotation_list_object),
                                   &annotation_list_object)) {
    LOG(WARNING) << "could not read annotations list object in " << name_;
    return;
  }

  process_types::Annotation<Traits> current = annotation_list_object.head;
  for (size_t index = 0;
       current.link_node != annotation_list_object.tail_pointer &&
       index < kMaxNumberOfAnnotations;
       ++index) {
    if (!memory_->Read(
            current.link_node, sizeof(current), &current)) {
      LOG(WARNING) << "could not read annotation at index " << index << " in " << name_;
      return;
    }

    if (current.size == 0) {
      continue;
    }

    AnnotationSnapshot snapshot;
    snapshot.type = current.type;

    char name[Annotation::kNameMaxLength];
    if (!memory_->Read(current.name, arraysize(name), name)) {
      LOG(WARNING) << "could not read annotation name at index " << index << "int " << name_;
      continue;
    }

    size_t name_length = strnlen(name, Annotation::kNameMaxLength);
    snapshot.name = std::string(name, name_length);

    size_t value_length =
        std::min(static_cast<size_t>(current.size), Annotation::kValueMaxSize);
    snapshot.value.resize(value_length);
    if (!memory_->Read(
            current.value, value_length, snapshot.value.data())) {
      LOG(WARNING) << "could not read annotation value at index " << index
                   << " in " << name_;
      continue;
    }

    vector_annotations->push_back(std::move(snapshot));
  }
}

}  // namespace crashpad
