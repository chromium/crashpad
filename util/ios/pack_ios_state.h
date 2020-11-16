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

#include "base/logging.h"

enum PackedObjectType { INVALID, DATA, MAP, LIST };

enum CommandType {
  MAP_START = 0x01,
  MAP_END = 0x02,
  ARRAY_START = 0x03,
  ARRAY_END = 0x04,
  PROPERTY = 0x05
};

class PackedMap;
class PackedData;
class PackedList;

class PackedObject {
 public:
  virtual PackedObjectType type() const = 0;
  virtual ~PackedObject() {}
  bool IsData() const { return type() == PackedObjectType::DATA; }
  bool IsMap() const { return type() == PackedObjectType::MAP; }
  bool IsList() const { return type() == PackedObjectType::LIST; }

  virtual void GetString(std::string* string) const {}
  virtual void GetInt(int* num) const {}

  virtual void GetBool(bool* val) const {}

  virtual bool IsEmpty() const { return true; }
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
  PackedData(const uint8_t* data, uint64_t length)
      : data_(data), length_(length) {}
  PackedObjectType type() const override { return DATA; }
  const PackedData& AsData() const override { return *this; }

  void GetString(std::string* string) const override {
    *string = std::string((const char*)data_, length_);
  }

  virtual void GetInt(int* val) const override { *val = (int)*data_; }

  virtual void GetBool(bool* val) const override { *val = (bool)*data_; }

  template <typename T>
  T* GetPointer() const {
    if (sizeof(T) == length_) {
      return (T*)data_;
    } else {
      // LOG ERROR.
    }
    return nullptr;
  }

  template <typename T>
  bool GetData(T* val) const {
    if (sizeof(T) == length_) {
      memcpy(val, data_, length_);
      return true;
    }
    return false;
  }

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
  inline void push_back(std::unique_ptr<const PackedMap> val) {
    list_.push_back(std::move(val));
  }

 private:
  std::vector<std::unique_ptr<const PackedMap>> list_;
};

class PackedMap : public PackedObject {
 public:
  static PackedMap Parse(const uint8_t* memory, off_t length);
  void DumpJson() const;

  PackedObjectType type() const override { return MAP; }
  const PackedMap& AsMap() const override { return *this; }
  bool HasKey(const std::string& key) const {
    return (map_.find(key) != map_.end());
  }
  const PackedObject& operator[](const std::string& key) const override {
    if (HasKey(key)) {
      return *map_.at(key).get();
    } else {
      return *empty_map_;
    }
  }

 private:
  void Dump(const PackedMap& map, int indent = 0) const;
  std::map<std::string, std::unique_ptr<PackedObject>> map_;
};
