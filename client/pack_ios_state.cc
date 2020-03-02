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

#include "pack_ios_state.h"

#include <stdio.h>
#include <memory>
#include <stack>
#include <vector>

enum CommandType {
  OBJECT_START = 0x01,
  OBJECT_END = 0x02,
  ARRAY_START = 0x03,
  ARRAY_END = 0x04,
  PROPERTY = 0x05
};

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
  uint8_t t = PROPERTY;
  write(fd, &t, sizeof(t));
  size_t name_length = strlen(name);
  write(fd, &name_length, sizeof(size_t));
  write(fd, name, name_length);
  write(fd, &value_length, sizeof(size_t));
  write(fd, value, value_length);
}

/* Parser to be called after restart. */

// So much unsafe memory reading here, neex to check bounds regularly.
const PackedData* PackedObject::empty = nullptr;
PackedMap Parse(const uint8_t* region, off_t length) {
  PackedMap mainDocument;
  std::stack<PackedObject*> parent;
  parent.push(&mainDocument);
  for (off_t command_index = 0; command_index < length;) {
    switch (region[command_index]) {
      case OBJECT_START: {
        command_index++;
        std::unique_ptr<PackedMap> newDocument(new PackedMap());
        if (parent.top()->IsObject()) {
          size_t name_length = static_cast<size_t>(region[command_index]);
          command_index += sizeof(size_t);
          const char* name =
              reinterpret_cast<const char*>(region + command_index);
          command_index += name_length;
          std::string str(name, name_length);
          auto parentMap = static_cast<PackedMap*>(parent.top());
          parent.push(newDocument.get());
          parentMap->map[str] = std::move(newDocument);
        } else {
          auto parentList = static_cast<PackedList*>(parent.top());
          parent.push(newDocument.get());
          parentList->list.push_back(std::move(newDocument));
        }
      } break;
      case OBJECT_END:
        command_index++;
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
        auto parentMap = static_cast<PackedMap*>(parent.top());
        parent.push(newList.get());
        parentMap->map[str] = std::move(newList);
      } break;
      case ARRAY_END:
        command_index++;
        parent.pop();
        break;
      case PROPERTY: {
        auto data = std::make_unique<PackedData>();
        command_index++;
        size_t name_length = static_cast<size_t>(region[command_index]);
        command_index += sizeof(size_t);
        const char* name =
            reinterpret_cast<const char*>(region + command_index);
        command_index += name_length;
        size_t value_length = static_cast<size_t>(region[command_index]);
        command_index += sizeof(size_t);
        data->length = value_length;
        data->data = region + command_index;
        command_index += value_length;
        std::string str(name, name_length);
        static_cast<PackedMap*>(parent.top())->map[str] = std::move(data);
      } break;
      default:
        // How do errors?
        return mainDocument;
    }
  }

  return mainDocument;
}
