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

#ifndef CRASHPAD_UTIL_IOS_IOS_MINIDUMP_OBJECT_H_
#define CRASHPAD_UTIL_IOS_IOS_MINIDUMP_OBJECT_H_

#include <stdint.h>
#include <unistd.h>

#include <map>
#include <string>
#include <vector>

namespace crashpad {
namespace internal {

enum IOSMinidumpObjectType { DATA, MAP, LIST };

class IOSMinidumpMap;
class IOSMinidumpData;
class IOSMinidumpList;

class IOSMinidumpObject {
 public:
  virtual IOSMinidumpObjectType type() const = 0;
  virtual ~IOSMinidumpObject() {}

  virtual void GetString(std::string* string) const {}
  virtual void GetInt(int* num) const {}
  virtual void GetBool(bool* val) const {}
  virtual const IOSMinidumpData& AsData() const;
  virtual const IOSMinidumpList& AsList() const;
  virtual const IOSMinidumpMap& AsMap() const;
  virtual const IOSMinidumpObject& operator[](const std::string& key) const {
    return *this;
  }
  IOSMinidumpObjectType type_;
};

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_UTIL_IOS_IOS_MINIDUMP_OBJECT_H_
