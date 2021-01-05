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

#include "util/ios/ios_intermediatedump_map.h"

#include <memory>
#include <stack>

#include "base/logging.h"
#include "util/ios/ios_intermediatedump_data.h"
#include "util/ios/ios_intermediatedump_list.h"
#include "util/ios/ios_intermediatedump_writer_util.h"

using crashpad::internal::IntermediateDumpKey;

namespace {

IntermediateDumpKey GetKey(const uint8_t* region,
                           off_t* command_index,
                           off_t max_length) {
  const uint64_t* raw_key =
      reinterpret_cast<const uint64_t*>(region + *command_index);
  *command_index += sizeof(uint64_t);
  if ((*command_index) > max_length) {
    DLOG(ERROR) << "Out of bounds key.";
    return IntermediateDumpKey::kInvalid;
  }
  return static_cast<IntermediateDumpKey>(*raw_key);
}

}  // namespace

namespace crashpad {
namespace internal {

IOSIntermediatedumpMap IOSIntermediatedumpMap::Parse(const uint8_t* region,
                                                     off_t max_length) {
  IOSIntermediatedumpMap mainDocument;
  std::stack<IOSIntermediatedumpObject*> parent;
  parent.push(&mainDocument);
  for (off_t command_index = 0; command_index < max_length;) {
    switch (region[command_index++]) {
      case MAP_START: {
        std::unique_ptr<IOSIntermediatedumpMap> newDocument(
            new IOSIntermediatedumpMap());
        if (parent.top()->type() == IOSIntermediatedumpObjectType::MAP) {
          auto parentMap = static_cast<IOSIntermediatedumpMap*>(parent.top());
          parent.push(newDocument.get());
          IntermediateDumpKey key = GetKey(region, &command_index, max_length);
          if (key == IntermediateDumpKey::kInvalid)
            return mainDocument;
          parentMap->map_[key] = std::move(newDocument);
        } else if (parent.top()->type() ==
                   IOSIntermediatedumpObjectType::LIST) {
          auto parentList = static_cast<IOSIntermediatedumpList*>(parent.top());
          parent.push(newDocument.get());
          parentList->push_back(std::move(newDocument));
        } else {
          DLOG(ERROR) << "Unexpected parent document.";
          return mainDocument;
        }
      } break;
      case ARRAY_START: {
        auto newList = std::make_unique<IOSIntermediatedumpList>();
        if (parent.top()->type() != IOSIntermediatedumpObjectType::MAP) {
          DLOG(ERROR) << "Attempting to push an array not in a map.";
          return mainDocument;
        }
        auto parentMap = static_cast<IOSIntermediatedumpMap*>(parent.top());
        parent.push(newList.get());
        IntermediateDumpKey key = GetKey(region, &command_index, max_length);
        if (key == IntermediateDumpKey::kInvalid)
          return mainDocument;
        parentMap->map_[key] = std::move(newList);
      } break;
      case MAP_END:
      case ARRAY_END:
        if (parent.size() < 2) {
          DLOG(ERROR) << "Attempting to pop off main document.";
          return mainDocument;
        }
        parent.pop();
        break;
      case PROPERTY: {
        IntermediateDumpKey key = GetKey(region, &command_index, max_length);
        if (key == IntermediateDumpKey::kInvalid)
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
        if (parent.top()->type() != IOSIntermediatedumpObjectType::MAP) {
          DLOG(ERROR) << "Attempting to add a property not in a map.";
          return mainDocument;
        }
        auto parentMap = static_cast<IOSIntermediatedumpMap*>(parent.top());
        parentMap->map_[key] =
            std::make_unique<IOSIntermediatedumpData>(data, *value_length);
      } break;
      default:
        DLOG(ERROR) << "Failed to parse serialized interim minidump.";
        return mainDocument;
    }
  }

  return mainDocument;
}

void IOSIntermediatedumpMap::DebugDump() const {
  DebugDump(*this);
}

#ifndef NDEBUG
std::ostream& operator<<(std::ostream& os, const IntermediateDumpKey& t) {
  switch (t) {
#define X(Name, VALUE)            \
  case IntermediateDumpKey::Name: \
    os << #Name;                  \
    break;
    INTERMEDIATE_DUMP_KEYS(X)
#undef X
  }
  return os;
}
void IOSIntermediatedumpMap::DebugDump(const IOSIntermediatedumpMap& map,
                                       int indent) const {
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
#endif

}  // namespace internal
}  // namespace crashpad
