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

#include "util/ios/ios_intermediate_dump_data.h"

namespace crashpad {
namespace internal {

bool IOSIntermediateDumpData::GetString(std::string* string) const {
  *string = std::string(reinterpret_cast<const char*>(data_.get()), length_);
  return true;
}

bool IOSIntermediateDumpData::GetInt(int* val) const {
  if (length_ != sizeof(int))
    return false;
  *val = static_cast<int>(*data_.get());
  return true;
}

bool IOSIntermediateDumpData::GetBool(bool* val) const {
  if (length_ != sizeof(bool))
    return false;
  *val = static_cast<bool>(data_.get());
  return true;
}

const uint8_t* IOSIntermediateDumpData::bytes() const {
  return data_.get();
}

uint64_t IOSIntermediateDumpData::length() const {
  return length_;
}

}  // namespace internal
}  // namespace crashpad
