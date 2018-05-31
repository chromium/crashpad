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

#include <algorithm>

namespace crashpad {

RangeSet::RangeSet() = default;

RangeSet::~RangeSet() = default;

void RangeSet::Insert(VMAddress base, VMSize size) {
  if (!size) {
    return;
  }

  VMAddress end = base + size - 1;

  // If there are no overlapping ranges, insert the new range.
  auto range_above_base = ranges_.lower_bound(base);
  if (range_above_base == ranges_.end() || range_above_base->second > end) {
    ranges_[end] = base;
    return;
  }

  if (range_above_base->first >= end) {
    // There is an existing range whose high address is greater than this
    // range's high address, so update the existing range.
    range_above_base->second = std::min(range_above_base->second, base);
  } else {
    auto range_above_end = ranges_.lower_bound(end);
    if (range_above_end != ranges_.end() && range_above_end->second <= end) {
      // If this range's high address reaches into another range, update that
      // range with the new base address.
      range_above_end->second = std::min(range_above_base->second, base);
    } else {
      ranges_[end] = std::min(range_above_base->second, base);
    }
    ranges_.erase(range_above_base);
  }
}

bool RangeSet::Contains(VMAddress address) const {
  auto range_above_address = ranges_.lower_bound(address);
  return range_above_address != ranges_.end() &&
         range_above_address->second <= address;
}

}  // namespace crashpad
