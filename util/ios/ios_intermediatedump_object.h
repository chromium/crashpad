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

#ifndef CRASHPAD_UTIL_IOS_IOS_INTERMEDIATEDUMP_OBJECT_H_
#define CRASHPAD_UTIL_IOS_IOS_INTERMEDIATEDUMP_OBJECT_H_

#include <stdint.h>
#include <unistd.h>

#include <map>
#include <string>
#include <vector>

#include "util/ios/ios_intermediatedump_writer_util.h"

namespace crashpad {
namespace internal {

enum IOSIntermediatedumpObjectType { DATA, MAP, LIST };

class IOSIntermediatedumpMap;
class IOSIntermediatedumpData;
class IOSIntermediatedumpList;

class IOSIntermediatedumpObject {
 public:
  virtual IOSIntermediatedumpObjectType type() const = 0;
  virtual ~IOSIntermediatedumpObject() {}

  virtual void GetString(std::string* string) const {}
  virtual void GetInt(int* num) const {}
  virtual void GetBool(bool* val) const {}
  virtual const IOSIntermediatedumpData& AsData() const;
  virtual const IOSIntermediatedumpList& AsList() const;
  virtual const IOSIntermediatedumpMap& AsMap() const;
  virtual const IOSIntermediatedumpObject& operator[](
      const IntermediateDumpKey& key) const {
    return *this;
  }
  IOSIntermediatedumpObjectType type_;
};

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_UTIL_IOS_IOS_INTERMEDIATEDUMP_OBJECT_H_
