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

#include <ostream>

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "util/ios/ios_intermediate_dump_data.h"
#include "util/ios/ios_intermediate_dump_map.h"

#ifndef CRASHPAD_SNAPSHOT_IOS_INTERMEDIATE_DUMP_READER_UTILS_H_
#define CRASHPAD_SNAPSHOT_IOS_INTERMEDIATE_DUMP_READER_UTILS_H_

namespace crashpad {

namespace internal {

// Overload the ostream output operator to make logged keys readable.
std::ostream& operator<<(std::ostream& os, const IntermediateDumpKey& t);

// Call GetAsData with error and UMA logging.
const IOSIntermediateDumpData* GetDataFromMap(const IOSIntermediateDumpMap* map,
                                              const IntermediateDumpKey& key);

// Call GetAsMap with error and UMA logging.
const IOSIntermediateDumpMap* GetMapFromMap(const IOSIntermediateDumpMap* map,
                                            const IntermediateDumpKey& key);

// Call GetAsList with error and UMA logging.
const IOSIntermediateDumpList* GetListFromMap(const IOSIntermediateDumpMap* map,
                                              const IntermediateDumpKey& key);

// Call GetAsList with error and UMA logging.
bool GetDataStringFromMap(const IOSIntermediateDumpMap* map,
                          const IntermediateDumpKey& key,
                          std::string* value);

// Call GetValue with error and UMA logging.
template <typename T>
bool GetDataValueFromMapIfExists(const IOSIntermediateDumpMap* map,
                                 const IntermediateDumpKey& key,
                                 T* value) {
  const IOSIntermediateDumpData* data = map->GetAsData(key);
  if (!data)
    return false;

  if (!data->GetValue(value)) {
    LOG(ERROR) << "Invalid key size: " << key;
    UMA_HISTOGRAM_ENUMERATION("Crashpad.IntermediateDump.Reader.InvalidKeySize",
                              key,
                              IntermediateDumpKey::kMaxValue);
    return false;
  }
  return true;
}

// Call GetDataFromMap and GetValue with error and UMA logging.

template <typename T>
bool GetDataValueFromMap(const IOSIntermediateDumpMap* map,
                         const IntermediateDumpKey& key,
                         T* value) {
  const IOSIntermediateDumpData* data = GetDataFromMap(map, key);
  if (!data) {
    return false;
  }

  if (!data->GetValue(value)) {
    LOG(ERROR) << "Invalid key size: " << key;
    UMA_HISTOGRAM_ENUMERATION("Crashpad.IntermediateDump.Reader.InvalidKeySize",
                              key,
                              IntermediateDumpKey::kMaxValue);
    return false;
  }
  return true;
}

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_IOS_INTERMEDIATE_DUMP_READER_UTILS_H_
