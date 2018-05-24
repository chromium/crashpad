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

#ifndef CRASHPAD_UTIL_MISC_RANGE_SET_H_
#define CRASHPAD_UTIL_MISC_RANGE_SET_H_

#include <utility>
#include <vector>

#include "base/macros.h"
#include "util/misc/address_types.h"

namespace crashpad {

//! \brief A set of VMAddress ranges.
class RangeSet {
 public:
  RangeSet();
  ~RangeSet();

  //! \brief Inserts a range into the set.
  //!
  //! \param[in] base The low addrees of the range.
  //! \param[in] size The size of the range.
  void Insert(VMAddress base, VMSize size);

  //! \brief Returns `true` if \a address falls within a range in this set.
  bool Contains(VMAddress address);

 private:
  // Use a Bloom filter to speed up address lookup.
  // The hash function is a direct mapping where bits 32:(32-kFilterAddressBits)
  // of an address are used as a bit-index into bloom_filter_.
  static constexpr size_t kFilterAddressBits = 11;

  char bloom_filter_[2 << (kFilterAddressBits - 3)];
  std::vector<std::pair<VMAddress, VMSize>> ranges_;

  DISALLOW_COPY_AND_ASSIGN(RangeSet);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_MISC_RANGE_SET_H_
