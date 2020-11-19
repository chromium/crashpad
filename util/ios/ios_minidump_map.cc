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

#include "util/ios/ios_minidump_map.h"

#include <memory>
#include <stack>

#include "base/logging.h"
#include "util/ios/ios_minidump_data.h"
#include "util/ios/ios_minidump_list.h"
#include "util/ios/ios_minidump_writer_util.h"

namespace {

std::string GetName(const uint8_t* region,
                    off_t* command_index,
                    off_t max_length) {
  const off_t* name_length =
      reinterpret_cast<const off_t*>(region + *command_index);
  *command_index += sizeof(off_t);
  if ((*command_index + *name_length) > max_length) {
    DLOG(ERROR) << "Out of bounds name.";
    return std::string();
  }
  const char* name = reinterpret_cast<const char*>(region + *command_index);
  *command_index += *name_length;
  return std::string(name, *name_length);
}

}  // namespace

namespace crashpad {
namespace internal {

IOSMinidumpMap IOSMinidumpMap::Parse(const uint8_t* region, off_t max_length) {
  IOSMinidumpMap mainDocument;
  std::stack<IOSMinidumpObject*> parent;
  parent.push(&mainDocument);
  for (off_t command_index = 0; command_index < max_length;) {
    switch (region[command_index++]) {
      case MAP_START: {
        std::unique_ptr<IOSMinidumpMap> newDocument(new IOSMinidumpMap());
        if (parent.top()->type() == IOSMinidumpObjectType::MAP) {
          auto parentMap = static_cast<IOSMinidumpMap*>(parent.top());
          parent.push(newDocument.get());
          std::string name = GetName(region, &command_index, max_length);
          if (name.empty())
            return mainDocument;
          parentMap->map_[name] = std::move(newDocument);
        } else {
          auto parentList = static_cast<IOSMinidumpList*>(parent.top());
          parent.push(newDocument.get());
          parentList->push_back(std::move(newDocument));
        }
      } break;
      case ARRAY_START: {
        auto newList = std::make_unique<IOSMinidumpList>();
        auto parentMap = static_cast<IOSMinidumpMap*>(parent.top());
        parent.push(newList.get());
        std::string name = GetName(region, &command_index, max_length);
        if (name.empty())
          return mainDocument;
        parentMap->map_[name] = std::move(newList);
      } break;
      case MAP_END:
      case ARRAY_END:
        parent.pop();
        break;
      case PROPERTY: {
        std::string name = GetName(region, &command_index, max_length);
        if (name.empty())
          return mainDocument;

        // Property length
        const off_t* value_length =
            reinterpret_cast<const off_t*>(region + command_index);
        command_index += sizeof(off_t);

        if ((command_index + *value_length) > max_length) {
          DLOG(ERROR) << "Out of bounds property value.";
          return mainDocument;
        }
        const uint8_t* data = region + command_index;
        command_index += *value_length;
        auto parentMap = static_cast<IOSMinidumpMap*>(parent.top());
        parentMap->map_[name] =
            std::make_unique<IOSMinidumpData>(data, *value_length);
      } break;
      default:
        DLOG(ERROR) << "Failed to parse serialized interim minidump.";
        return mainDocument;
    }
  }

  return mainDocument;
}

void IOSMinidumpMap::DebugDump() const {
  DebugDump(*this);
}

void IOSMinidumpMap::DebugDump(const IOSMinidumpMap& map, int indent) const {
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

}  // namespace internal
}  // namespace crashpad
