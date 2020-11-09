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

enum PackedObjectType { INVALID, DATA, OBJECT, LIST };

enum CommandType {
  OBJECT_START = 0x01,
  OBJECT_END = 0x02,
  ARRAY_START = 0x03,
  ARRAY_END = 0x04,
  PROPERTY = 0x05
};

/*
class PackedValue;

typedef std::vector<uint8_t*> PackedData;
typedef std::map<std::string, const PackedValue*> PackedObject;
typedef std::vector<PackedObject*> PackedList;

class PackedValue {
  public:
  PackedValue(uint8_t* data, const uint64_t length) {
    type_ = DATA;
    data_value_.data = data;
    data_value_.length = length;
  }
  PackedValue(const PackedObject *object) {
    type_ = OBJECT;
    object_value_ = object;
  }

  PackedValue(const PackedList *list) {
    type_ = LIST;
    list_value_ = list;
  }
  ~PackedValue();

    static PackedObject Parse(const uint8_t* data, off_t length);

    bool IsData() const { return type_ == DATA; }
    bool IsList() const { return type_ == LIST; }
    bool IsObject() const { return type_ == OBJECT; }

  const PackedData* AsData() const { return new PackedData(data_value_.data, data_value_.length); }
  const PackedList &AsList() const { return *list_value_; }
  const PackedObject &AsObject() const { return *object_value_; }

  private:

  PackedObjectType type_;
  union
  {
    struct {
      uint8_t* data;
      uint64_t length;
    } data_value_;
    const PackedList* list_value_;
    const PackedObject* object_value_;
  };

};
*/

class PackedMap;
class PackedData;
class PackedList;

class PackedObject {
 public:
  virtual PackedObjectType type() const = 0;
  virtual ~PackedObject() {}
  bool IsData() { return type() == PackedObjectType::DATA; }
  bool IsObject() { return type() == PackedObjectType::OBJECT; }
  bool IsList() { return type() == PackedObjectType::LIST; }

  virtual const PackedData& AsData() const { return *empty_data_; }
  virtual const PackedList& AsList() const { return *empty_list_; }
  virtual const PackedMap& AsMap() const { return *empty_map_; }
  virtual const PackedObject& operator[](const std::string& key) const {
    return *this;
  }
  PackedObjectType type_;
  static const PackedData* empty_data_;
  static const PackedList* empty_list_;
  static const PackedMap* empty_map_;
};

class PackedData : public PackedObject {
 public:
  PackedData(const uint8_t* data, uint64_t length) :data_(data), length_(length) {}
  PackedObjectType type() const override { return DATA; }
  const PackedData& AsData() const override { return *this; }
  
  uint64_t length() const { return length_; }
  const uint8_t* data() const { return data_; }
private:
  const uint8_t* data_;
  const uint64_t length_;
};

class PackedList : public PackedObject {
 public:
  PackedObjectType type() const override { return LIST; }
  const PackedList& AsList() const override { return *this; }
  inline auto begin() const noexcept { return list_.begin(); }
  inline auto end() const noexcept { return list_.end(); }
  inline void push_back(std::unique_ptr<const PackedMap> val) { list_.push_back(std::move(val)); }
private:
  std::vector<std::unique_ptr<const PackedMap>> list_;
};

class PackedMap : public PackedObject {
 public:
  static PackedMap Parse(const uint8_t* memory, off_t length);
  
  void DumpJson() const;

  PackedObjectType type() const override { return OBJECT; }
  const PackedMap& AsMap() const override { return *this; }
  const PackedObject& operator[](const std::string& key) const override {
    if (*map_.find(key) != *map_.end()) {
      return *map_.at(key).get();
    } else {
      return *empty_map_;
    }
  }
private:
  void Dump(const PackedMap& map) const;
  std::map<std::string, std::unique_ptr<PackedObject>> map_;
};

void ObjectStart(int fd, const char* name);
void ArrayObjectStart(int fd);
void Property(int fd, const char* name, const void* value, size_t value_length);
void ArrayStart(int fd, const char* name);
void ObjectEnd(int fd);
void ArrayEnd(int fd);
