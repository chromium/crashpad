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

std::string GetName(const uint8_t* region, off_t *command_index) {
  size_t name_length = static_cast<size_t>(region[*command_index]);
  *command_index += sizeof(size_t);
  const char* name =
      reinterpret_cast<const char*>(region + *command_index);
  *command_index += name_length;
  return std::string(name, name_length);
}

}

/* No Allocation In-Handler Safe Methods */

void ArrayObjectStart(int fd) {
  uint8_t t = OBJECT_START;
  write(fd, &t, sizeof(uint8_t));
}

void ObjectStart(int fd, const char* name) {
  uint8_t t = OBJECT_START;
  write(fd, &t, sizeof(uint8_t));
  size_t name_length = strlen(name);
  write(fd, &name_length, sizeof(size_t));
  write(fd, name, name_length);
}

void ArrayStart(int fd, const char* name) {
  uint8_t t = ARRAY_START;
  write(fd, &t, sizeof(uint8_t));
  size_t name_length = strlen(name);
  write(fd, &name_length, sizeof(size_t));
  write(fd, name, name_length);
}

void ObjectEnd(int fd) {
  uint8_t t = OBJECT_END;
  write(fd, &t, sizeof(uint8_t));
}

void ArrayEnd(int fd) {
  uint8_t t = ARRAY_END;
  write(fd, &t, sizeof(uint8_t));
}

void Property(int fd,
              const char* name,
              const void* value,
              size_t value_length) {
//  DLOG(INFO) << "writing " << name;

  uint8_t t = PROPERTY;
  write(fd, &t, sizeof(t));
  size_t name_length = strlen(name);
  write(fd, &name_length, sizeof(size_t));
  write(fd, name, name_length);
  off_t value_length_offset = lseek(fd, 0, SEEK_CUR);
  write(fd, &value_length, sizeof(size_t));
  ssize_t actual_length = write(fd, value, value_length);
  if (actual_length != (ssize_t)value_length) {
    DLOG(INFO) << strerror(errno);
    // seek back and zero out this property.
    lseek(fd, value_length_offset, SEEK_SET);
    constexpr size_t zero = 0;
    actual_length = write(fd, &zero, sizeof(size_t));
  }
}

/*
PackedObject PackedValue::Parse(const uint8_t* region, off_t length) {
  PackedObject mainDocument;
  std::stack<PackedValue*> parent;
  parent.push(&mainDocument);
  
  for (off_t command_index = 0; command_index < length;) {
    switch (region[command_index++]) {
      case OBJECT_START: {
        std::unique_ptr<PackedObject> newDocument(new PackedObject());
        if (parent.top()->IsObject()) {
          size_t name_length = static_cast<size_t>(region[command_index]);
          command_index += sizeof(size_t);
          const char* name =
              reinterpret_cast<const char*>(region + command_index);
          command_index += name_length;
          std::string str(name, name_length);
          DLOG(INFO) << "Adding object " << str;
          auto parentMap = static_cast<PackedObject*>(parent.top());
          parent.push(newDocument.get());
          parentMap->map[str] = std::move(newDocument);
        } else {
          auto parentList = static_cast<PackedList*>(parent.top());
          parent.push(newDocument.get());
          parentList->list.push_back(std::move(newDocument));
        }
      } break;
      case OBJECT_END:
        parent.pop();
        break;
      case ARRAY_START: {
        auto newList = std::make_unique<PackedList>();
        command_index++;
        size_t name_length = static_cast<size_t>(region[command_index]);
        command_index += sizeof(size_t);
        const char* name =
            reinterpret_cast<const char*>(region + command_index);
        command_index += name_length;
        std::string str(name, name_length);
        DLOG(INFO) << "Adding array " << str;
        auto parentMap = static_cast<PackedObject*>(parent.top());
        parent.push(newList.get());
        parentMap->map[str] = std::move(newList);
      } break;
      case ARRAY_END:
        parent.pop();
        break;
      case PROPERTY: {
        auto data = std::make_unique<PackedData>();
        size_t name_length = static_cast<size_t>(region[command_index]);
        command_index += sizeof(size_t);
        const char* name =
            reinterpret_cast<const char*>(region + command_index);
        std::string str(name, name_length);
        DLOG(INFO) << "Adding property " << str;

        command_index += name_length;
        size_t* value_length = (size_t*)(region + command_index);
        command_index += sizeof(size_t);
        data->length = *value_length;
        data->data = region + command_index;
        command_index += *value_length;
        static_cast<PackedObject*>(parent.top())->map[str] = std::move(data);
      } break;
      default:
        // How do errors?
        DLOG(INFO) << "Failing with document. ";
        return mainDocument;
    }
  }
  return mainDocument;
}
 */
/* Parser to be called after restart. */

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
      case OBJECT_START: {
        std::unique_ptr<PackedMap> newDocument(new PackedMap());
        if (parent.top()->IsObject()) {
          auto parentMap = static_cast<PackedMap*>(parent.top());
          parent.push(newDocument.get());
          std::string name = GetName(region, &command_index);
//          DLOG(INFO) << "Adding object " << name;
          parentMap->map_[name] = std::move(newDocument);
        } else {
          auto parentList = static_cast<PackedList*>(parent.top());
          parent.push(newDocument.get());
//          DLOG(INFO) << "Adding unnamed list item";
          parentList->push_back(std::move(newDocument));
        }
      } break;
      case ARRAY_START: {
        auto newList = std::make_unique<PackedList>();
        auto parentMap = static_cast<PackedMap*>(parent.top());
        parent.push(newList.get());
        std::string name = GetName(region, &command_index);
//        DLOG(INFO) << "Adding array named " << name;
        parentMap->map_[name] = std::move(newList);
      } break;
      case OBJECT_END:
      case ARRAY_END:
        parent.pop();
        break;
      case PROPERTY: {
        std::string name = GetName(region, &command_index);
//        DLOG(INFO) << "Adding property " << name;

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
        // How do errors?
        DLOG(INFO) << "Failing with document. ";
        return mainDocument;
    }
  }

  return mainDocument;
}

void PackedMap::DumpJson() const {
  Dump(*this);
}

void PackedMap::Dump(const PackedMap& map) const {
  for (auto const& x : map.map_)
  {
    
    if (x.second->type() == DATA) {
      DLOG(INFO) << x.first << " : value skipped";
    }
    if (x.second->type() == OBJECT) {
      DLOG(INFO) << x.first << " : {";
      Dump(x.second->AsMap());
      DLOG(INFO) << "}";
    }
    if (x.second->type() == LIST) {
      DLOG(INFO) << x.first << " : [";
      for(auto& value : x.second->AsList()) {
        DLOG(INFO) << "{";
        Dump(value->AsMap());
        DLOG(INFO) << "},";
        break; // don't care.
      }
      DLOG(INFO) << "]";

    }
  }
}
