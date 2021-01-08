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

#ifndef CRASHPAD_UTIL_IOS_PACK_IOS_MAP_H_
#define CRASHPAD_UTIL_IOS_PACK_IOS_MAP_H_

#include <stdint.h>
#include <unistd.h>

#include <map>
#include <string>
#include <vector>

#include "util/file/file_reader.h"
#include "util/ios/ios_intermediatedump_object.h"
#include "util/ios/ios_intermediatedump_writer.h"

namespace crashpad {
namespace internal {

class IOSIntermediatedumpMap : public IOSIntermediatedumpObject {
 public:
  static bool Parse(FileReaderInterface* file_reader,
                    IOSIntermediatedumpMap& mainDocument);
  void DebugDump() const;

  IOSIntermediatedumpObjectType type() const override { return MAP; }
  const IOSIntermediatedumpMap& AsMap() const override { return *this; }
  bool HasKey(const IntermediateDumpKey& key) const {
    return (map_.find(key) != map_.end());
  }
  const IOSIntermediatedumpObject& operator[](
      const IntermediateDumpKey& key) const override {
    if (HasKey(key)) {
      return *map_.at(key).get();
    } else {
      static const auto* empty_map = new IOSIntermediatedumpMap();
      return *empty_map;
    }
  }

 private:
#ifndef NDEBUG
  void DebugDump(const IOSIntermediatedumpMap& map, int indent = 0) const;
#endif
  std::map<IntermediateDumpKey, std::unique_ptr<IOSIntermediatedumpObject>>
      map_;
};

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_UTIL_IOS_PACK_IOS_MAP_H_
