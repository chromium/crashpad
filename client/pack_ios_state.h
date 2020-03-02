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

#pragma once

#include <stdint.h>
#include <unistd.h>
#include <map>
#include <string>
#include <vector>

enum PackedObjectType { DATA, OBJECT, LIST };

class PackedMap;
class PackedData;

class PackedObject {
 public:
  virtual PackedObjectType type() const = 0;
  virtual ~PackedObject() {}
  bool IsData() { return type() == PackedObjectType::DATA; }
  bool IsObject() { return type() == PackedObjectType::OBJECT; }
  bool IsList() { return type() == PackedObjectType::LIST; }

  static const PackedData* empty;
  virtual const PackedData& get_data() const { return *empty; }
  virtual const PackedObject& operator[](size_t i) const { return *this; }
  virtual const PackedObject& operator[](const std::string& key) const {
    return *this;
  }
  PackedObjectType type_;
};

class PackedMap : public PackedObject {
 public:
  PackedObjectType type() const override { return OBJECT; }

  std::map<std::string, std::unique_ptr<PackedObject>> map;
  const PackedObject& operator[](const std::string& key) const override {
    return *map.at(key).get();
  }
};

class PackedData : public PackedObject {
 public:
  PackedObjectType type() const override { return DATA; }
  const PackedData& get_data() const override { return *this; }
  uint64_t length;
  const uint8_t* data;
};

class PackedList : public PackedObject {
 public:
  PackedObjectType type() const override { return LIST; }

  std::vector<std::unique_ptr<const PackedMap>> list;
  const PackedObject& operator[](size_t i) const override { return *list[i]; }
};

void ObjectStart(int fd, const char* name);
void ArrayObjectStart(int fd);
void Property(int fd, const char* name, const void* value, size_t value_length);
void ArrayStart(int fd, const char* name);
void ObjectEnd(int fd);
void ArrayEnd(int fd);

PackedMap Parse(const uint8_t* memory, off_t length);
