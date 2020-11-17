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

#include <memory>
#include <stack>

#include "base/logging.h"

namespace {

std::string GetName(const uint8_t* region, off_t* command_index, off_t length) {
  off_t name_length = static_cast<off_t>(region[*command_index]);
  *command_index += sizeof(off_t);
  if ((*command_index + name_length) > length) {
    DLOG(ERROR) << "Out of bounds name.";
    return std::string();
  }
  const char* name = reinterpret_cast<const char*>(region + *command_index);
  *command_index += name_length;
  return std::string(name, name_length);
}

}  // namespace

namespace crashpad {

const PackedData& PackedObject::AsData() const {
  static const PackedData* empty_data = new PackedData(0, 0);
  return *empty_data;
}
const PackedList& PackedObject::AsList() const {
  static const PackedList* empty_list = new PackedList();
  return *empty_list;
}
const PackedMap& PackedObject::AsMap() const {
  static const auto* empty_map = new PackedMap();
  return *empty_map;
}

PackedMap PackedMap::Parse(const uint8_t* region, off_t length) {
  PackedMap mainDocument;
  std::stack<PackedObject*> parent;
  parent.push(&mainDocument);
  for (off_t command_index = 0; command_index < length;) {
    switch (region[command_index++]) {
      case MAP_START: {
        std::unique_ptr<PackedMap> newDocument(new PackedMap());
        if (parent.top()->type() == PackedObjectType::MAP) {
          auto parentMap = static_cast<PackedMap*>(parent.top());
          parent.push(newDocument.get());
          std::string name = GetName(region, &command_index, length);
          if (name.empty())
            return mainDocument;
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
        std::string name = GetName(region, &command_index, length);
        if (name.empty())
          return mainDocument;
        parentMap->map_[name] = std::move(newList);
      } break;
      case MAP_END:
      case ARRAY_END:
        parent.pop();
        break;
      case PROPERTY: {
        std::string name = GetName(region, &command_index, length);
        if (name.empty())
          return mainDocument;

        // Property length
        const off_t* value_length =
            reinterpret_cast<const off_t*>(region + command_index);
        command_index += sizeof(off_t);

        if ((command_index + *value_length) > length) {
          DLOG(ERROR) << "Out of bounds property value.";
          return mainDocument;
        }
        const uint8_t* data = region + command_index;
        command_index += *value_length;
        auto parentMap = static_cast<PackedMap*>(parent.top());
        parentMap->map_[name] =
            std::make_unique<PackedData>(data, *value_length);
      } break;
      default:
        DLOG(ERROR) << "Failed to parse serialized interim minidump.";
        return mainDocument;
    }
  }

  return mainDocument;
}

void PackedMap::DebugDump() const {
  DebugDump(*this);
}

void PackedMap::DebugDump(const PackedMap& map, int indent) const {
  for (auto const& x : map.map_) {
    if (x.second->type() == DATA) {
      DLOG(INFO) << std::string(indent, ' ') << "\"" << x.first
                 << "\" : " << x.second->AsData().length() << ",";
    }
    if (x.second->type() == MAP) {
      DLOG(INFO) << std::string(indent, ' ') << "\"" << x.first << "\" : {";
      DebugDump(x.second->AsMap(), indent + 2);
      DLOG(INFO) << std::string(indent, ' ') << "},";
    }
    if (x.second->type() == LIST) {
      DLOG(INFO) << std::string(indent, ' ') << "\"" << x.first << "\" : [";
      for (auto& value : x.second->AsList()) {
        DLOG(INFO) << std::string(indent + 1, ' ') << "{";
        DebugDump(value->AsMap(), indent + 2);
        DLOG(INFO) << std::string(indent + 1, ' ') << "},";
      }
      DLOG(INFO) << std::string(indent, ' ') << "],";
    }
  }
}

}  // namespace crashpad
