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
#include "util/ios/ios_intermediatedump_writer.h"

using crashpad::internal::IntermediateDumpKey;

namespace crashpad {
namespace internal {

bool IOSIntermediatedumpMap::Parse(FileReaderInterface* reader,
                                   IOSIntermediatedumpMap& mainDocument) {
  if (!reader->SeekSet(0)) {
    return false;
  }
  std::stack<IOSIntermediatedumpObject*> parent;
  parent.push(&mainDocument);
  CommandType command;

  while (reader->ReadExactly(&command, sizeof(CommandType))) {
    switch (command) {
      case MAP_START: {
        std::unique_ptr<IOSIntermediatedumpMap> newDocument(
            new IOSIntermediatedumpMap());
        if (parent.top()->type() == IOSIntermediatedumpObjectType::MAP) {
          auto parentMap = static_cast<IOSIntermediatedumpMap*>(parent.top());
          parent.push(newDocument.get());
          IntermediateDumpKey key;
          if (!reader->ReadExactly(&key, sizeof(key)))
            return false;
          if (key == IntermediateDumpKey::kInvalid)
            return false;
          parentMap->map_[key] = std::move(newDocument);
        } else if (parent.top()->type() ==
                   IOSIntermediatedumpObjectType::LIST) {
          auto parentList = static_cast<IOSIntermediatedumpList*>(parent.top());
          parent.push(newDocument.get());
          parentList->push_back(std::move(newDocument));
        } else {
          DLOG(ERROR) << "Unexpected parent document.";
          return false;
        }
      } break;
      case ARRAY_START: {
        auto newList = std::make_unique<IOSIntermediatedumpList>();
        if (parent.top()->type() != IOSIntermediatedumpObjectType::MAP) {
          DLOG(ERROR) << "Attempting to push an array not in a map.";
          return false;
        }
        auto parentMap = static_cast<IOSIntermediatedumpMap*>(parent.top());
        parent.push(newList.get());
        IntermediateDumpKey key;
        if (!reader->ReadExactly(&key, sizeof(key)))
          return false;
        if (key == IntermediateDumpKey::kInvalid)
          return false;
        parentMap->map_[key] = std::move(newList);
      } break;
      case MAP_END:
      case ARRAY_END:
        if (parent.size() < 2) {
          DLOG(ERROR) << "Attempting to pop off main document.";
          return false;
        }
        parent.pop();
        break;
      case PROPERTY: {
        if (parent.top()->type() != IOSIntermediatedumpObjectType::MAP) {
          DLOG(ERROR) << "Attempting to add a property not in a map.";
          return false;
        }
        IntermediateDumpKey key;
        if (!reader->ReadExactly(&key, sizeof(key)))
          return false;
        if (key == IntermediateDumpKey::kInvalid)
          return false;

        off_t value_length;
        if (!reader->ReadExactly(&value_length, sizeof(value_length))) {
          return false;
        }

        auto data = std::make_unique<uint8_t[]>(value_length);
        if (!reader->ReadExactly(data.get(), value_length)) {
          return false;
        }
        auto parentMap = static_cast<IOSIntermediatedumpMap*>(parent.top());
        parentMap->map_[key] = std::make_unique<IOSIntermediatedumpData>(
            std::move(data), value_length);
      } break;
      case DOCUMENT_END: {
        if (parent.size() != 1) {
          DLOG(ERROR) << "Unexpected end of main document.";
          return false;
        }
        return true;
      }
      default:
        DLOG(ERROR) << "Failed to parse serialized interim minidump.";
        return false;
    }
  }

  DLOG(ERROR) << "Unexpected end of main document.";
  return false;
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
