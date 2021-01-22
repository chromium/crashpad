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

#include "util/ios/ios_intermediatedump_object.h"
#include "util/ios/ios_intermediatedump_data.h"
#include "util/ios/ios_intermediatedump_list.h"
#include "util/ios/ios_intermediatedump_map.h"

namespace crashpad {
namespace internal {

const IOSIntermediatedumpData& IOSIntermediatedumpObject::AsData() const {
  static const IOSIntermediatedumpData* empty_data =
      new IOSIntermediatedumpData(0, 0);
  return *empty_data;
}
const IOSIntermediatedumpList& IOSIntermediatedumpObject::AsList() const {
  static const IOSIntermediatedumpList* empty_list =
      new IOSIntermediatedumpList();
  return *empty_list;
}
const IOSIntermediatedumpMap& IOSIntermediatedumpObject::AsMap() const {
  static const auto* empty_map = new IOSIntermediatedumpMap();
  return *empty_map;
}

}  // namespace internal
}  // namespace crashpad
