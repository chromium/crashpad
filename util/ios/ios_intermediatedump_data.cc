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

#include "util/ios/ios_intermediatedump_data.h"

namespace crashpad {
namespace internal {

IOSIntermediatedumpObjectType IOSIntermediatedumpData::type() const {
  return DATA;
}

const IOSIntermediatedumpData& IOSIntermediatedumpData::AsData() const {
  return *this;
}
void IOSIntermediatedumpData::GetString(std::string* string) const {
  *string = std::string(reinterpret_cast<const char*>(data_.get()), length_);
}

void IOSIntermediatedumpData::GetInt(int* val) const {
  *val = static_cast<int>(*data_.get());
}
void IOSIntermediatedumpData::GetBool(bool* val) const {
  *val = static_cast<bool>(data_.get());
}

const uint8_t* IOSIntermediatedumpData::bytes() const {
  return data_.get();
}

uint64_t IOSIntermediatedumpData::length() const {
  return length_;
}

}  // namespace internal
}  // namespace crashpad
