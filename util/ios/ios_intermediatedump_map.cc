// Copyright 2021 The Crashpad Authors. All rights reserved.
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

#include "base/logging.h"
#include "util/ios/ios_intermediatedump_data.h"
#include "util/ios/ios_intermediatedump_list.h"

using crashpad::internal::IntermediateDumpKey;

namespace crashpad {
namespace internal {

bool IOSIntermediatedumpMap::HasKey(const IntermediateDumpKey& key) const {
  return (map_.find(key) != map_.end());
}

const IOSIntermediatedumpObject& IOSIntermediatedumpMap::operator[](
    const IntermediateDumpKey& key) const {
  if (HasKey(key)) {
    return *map_.at(key).get();
  } else {
    static const auto* empty_map = new IOSIntermediatedumpMap();
    return *empty_map;
  }
}

#ifndef NDEBUG
void IOSIntermediatedumpMap::DebugDump() const {
  DebugDump(*this);
}

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
