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

#include "snapshot/ios/intermediate_dump_reader_util.h"

#include "util/ios/ios_intermediate_dump_data.h"
#include "util/ios/ios_intermediate_dump_map.h"

namespace crashpad {
namespace internal {

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

const IOSIntermediateDumpData* GetDataFromMap(const IOSIntermediateDumpMap* map,
                                              const IntermediateDumpKey& key) {
  const auto data = map->GetAsData(key);
  if (!data) {
    LOG(ERROR) << "Missing expected data key " << key;
    UMA_HISTOGRAM_ENUMERATION("Crashpad.IntermediateDump.Reader.MissingKey",
                              key,
                              IntermediateDumpKey::kMaxValue);
    return nullptr;
  }
  return data;
}

const IOSIntermediateDumpMap* GetMapFromMap(const IOSIntermediateDumpMap* map,
                                            const IntermediateDumpKey& key) {
  const auto map_dump = map->GetAsMap(key);
  if (!map_dump) {
    LOG(ERROR) << "Missing expected map key " << key;
    UMA_HISTOGRAM_ENUMERATION("Crashpad.IntermediateDump.Reader.MissingKey",
                              key,
                              IntermediateDumpKey::kMaxValue);
    return nullptr;
  }
  return map_dump;
}

const IOSIntermediateDumpList* GetListFromMap(const IOSIntermediateDumpMap* map,
                                              const IntermediateDumpKey& key) {
  const auto list = map->GetAsList(key);
  if (!list) {
    LOG(ERROR) << "Missing expected list key " << key;
    UMA_HISTOGRAM_ENUMERATION("Crashpad.IntermediateDump.Reader.MissingKey",
                              key,
                              IntermediateDumpKey::kMaxValue);
    return nullptr;
  }
  return list;
}

bool GetDataStringFromMap(const IOSIntermediateDumpMap* map,
                          const IntermediateDumpKey& key,
                          std::string* value) {
  auto data = GetDataFromMap(map, key);
  if (!data) {
    LOG(ERROR) << "Missing expected string key " << key;
    UMA_HISTOGRAM_ENUMERATION("Crashpad.IntermediateDump.Reader.MissingKey",
                              key,
                              IntermediateDumpKey::kMaxValue);
    return false;
  }

  *value = data->GetString();
  return true;
}

}  // namespace internal
}  // namespace crashpad
