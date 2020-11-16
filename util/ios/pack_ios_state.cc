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

#include "util/ios/pack_ios_state.h"

#include <stdio.h>
#include <memory>
#include <stack>
#include <vector>

#include "base/logging.h"

namespace {

std::string GetName(const uint8_t* region, off_t* command_index) {
  size_t name_length = static_cast<size_t>(region[*command_index]);
  *command_index += sizeof(size_t);
  const char* name = reinterpret_cast<const char*>(region + *command_index);
  *command_index += name_length;
  return std::string(name, name_length);
}

}  // namespace

// So much unsafe memory reading here, neex to check bounds regularly.

const PackedData* PackedObject::empty_data_ = new PackedData(0, 0);
const PackedMap* PackedObject::empty_map_ = new PackedMap();
const PackedList* PackedObject::empty_list_ = new PackedList();
PackedMap PackedMap::Parse(const uint8_t* region, off_t length) {
  PackedMap mainDocument;
  std::stack<PackedObject*> parent;
  parent.push(&mainDocument);
  for (off_t command_index = 0; command_index < length;) {
    switch (region[command_index++]) {
      case MAP_START: {
        std::unique_ptr<PackedMap> newDocument(new PackedMap());
        if (parent.top()->IsMap()) {
          auto parentMap = static_cast<PackedMap*>(parent.top());
          parent.push(newDocument.get());
          std::string name = GetName(region, &command_index);
          parentMap->map_[name] = std::move(newDocument);
        } else {
          auto parentList = static_cast<PackedList*>(parent.top());
          parent.push(newDocument.get());
          parentList->push_back(std::move(newDocument));
        }
      } break;
      case ARRAY_START: {
        auto newList = std::make_unique<PackedList>();
        auto parentMap = static_cast<PackedMap*>(parent.top());
        parent.push(newList.get());
        std::string name = GetName(region, &command_index);
        parentMap->map_[name] = std::move(newList);
      } break;
      case MAP_END:
      case ARRAY_END:
        parent.pop();
        break;
      case PROPERTY: {
        std::string name = GetName(region, &command_index);

        // Property length
        size_t* value_length = (size_t*)(region + command_index);
        command_index += sizeof(size_t);

        // Pointer to beginning of data.
        uint64_t length = *value_length;
        const uint8_t* data = region + command_index;
        command_index += *value_length;
        auto parentMap = static_cast<PackedMap*>(parent.top());
        parentMap->map_[name] = std::make_unique<PackedData>(data, length);
      } break;
      default:
        DLOG(INFO) << "Failed to parse serialized intermediate minidump.";
        return mainDocument;
    }
  }

  return mainDocument;
}

void PackedMap::DumpJson() const {
  Dump(*this);
}

void PackedMap::Dump(const PackedMap& map, int indent) const {
  for (auto const& x : map.map_) {
    if (x.second->type() == DATA) {
      DLOG(INFO) << std::string(indent, ' ') << "\"" << x.first
                 << "\" : " << x.second->AsData().length() << ",";
    }
    if (x.second->type() == MAP) {
      DLOG(INFO) << std::string(indent, ' ') << "\"" << x.first << "\" : {";
      Dump(x.second->AsMap(), indent + 2);
      DLOG(INFO) << std::string(indent, ' ') << "},";
    }
    if (x.second->type() == LIST) {
      DLOG(INFO) << std::string(indent, ' ') << "\"" << x.first << "\" : [";
      for (auto& value : x.second->AsList()) {
        DLOG(INFO) << std::string(indent + 1, ' ') << "{";
        Dump(value->AsMap(), indent + 2);
        DLOG(INFO) << std::string(indent + 1, ' ') << "},";
        break;  // don't care.
      }
      DLOG(INFO) << std::string(indent, ' ') << "],";
    }
  }
}
