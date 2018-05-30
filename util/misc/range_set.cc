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

#include "base/logging.h"

namespace crashpad {

RangeSet::RangeSet() : ranges_() {
  memset(bloom_filter_, 0, sizeof(bloom_filter_));
}

RangeSet::~RangeSet() = default;

// Extract the bit field 32:(32-kFilterAddressBits) from address.
#define FILTER_INDEX(address) \
  ((address & 0xffffffff) >> (32 - kFilterAddressBits))

// Set the `index`th bit in bloom_filter_ to 1.
// index >> 3 converts a the bit index to a byte index.
// 1 << (index & 7) selects the bit in that byte.
#define SET_FILTER_BIT(index)                      \
  do {                                             \
    bloom_filter_[index >> 3] |= 1 << (index & 7); \
  } while (0)

// Return the `index`th bit in bloom_filter_.
#define GET_FILTER_BIT(index) (bloom_filter_[index >> 3] & (1 << (index & 7)))

void RangeSet::Insert(VMAddress base, VMSize size) {
  if (!size) {
    return;
  }

  ranges_.push_back(std::make_pair(base, size));

  for (size_t bit_index = FILTER_INDEX(base);
       bit_index <= FILTER_INDEX(base + size - 1);
       ++bit_index) {
    SET_FILTER_BIT(bit_index);
  }
}

bool RangeSet::Contains(VMAddress address) const {
  if (!GET_FILTER_BIT(FILTER_INDEX(address))) {
    return false;
  }

  for (const auto& range : ranges_) {
    if (address >= range.first && address < range.first + range.second) {
      return true;
    }
  }
  return false;
}

}  // namespace crashpad
