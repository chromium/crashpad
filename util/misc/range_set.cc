// Copyright 2018 The Crashpad Authors. All rights reserved.
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

#include "util/misc/range_set.h"

namespace crashpad {

void RangeSet::Insert(VMAddress base, VMSize size) {
  ranges_.push_back(std::make_pair(base, size));
}

bool RangeSet::Contains(VMAddress address) {
  for (const auto& range : ranges_) {
    if (address >= range.first && address < range.first + range.second) {
      return true;
    }
  }
  return false;
}

}  // namespace crashpad
