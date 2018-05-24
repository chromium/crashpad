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

#ifndef CRASHPAD_SNAPSHOT_SANITIZED_STACK_SNAPSHOT_SANITIZED_H_
#define CRASHPAD_SNAPSHOT_SANITIZED_STACK_SNAPSHOT_SANITIZED_H_

#include <stdint.h>

#include "snapshot/memory_snapshot.h"
#include "util/misc/range_set.h"

namespace crashpad {
namespace internal {

//! \brief A MemorySnapshot which wraps and filters sensitive information from
//!     another MemorySnapshot.
//!
//! This class redacts all data from the wrapped MemorySnapshot unless:
//! 1. The data looks like a pointer. The data is pointer aligned and points
//!     into a whitelisted address range.
//! 2. The data is pointer aligned and is a small integer.
class StackSnapshotSanitized final : public MemorySnapshot {
 public:
  //! \brief Redacted data is replaced with this value, casted to the
  //!     appropriate size for a pointer in the target process.
  static constexpr uint64_t kDefaced = 0x0defaced0defaced;

  //! \brief Words smaller than this value are not redacted.
  static constexpr uint64_t kSmallWordMax = 4096;

  //! \brief Constructs this object.
  //!
  //! \param[in] stack The MemorySnapshot to sanitize.
  //! \param[in] ranges A set of whitelisted address ranges.
  //! \param[in] is_64_bit `true` if this stack is for a 64-bit process.
  StackSnapshotSanitized(const MemorySnapshot* stack,
                         RangeSet* ranges,
                         bool is_64_bit);

  ~StackSnapshotSanitized() override;

  // MemorySnapshot:

  uint64_t Address() const override;
  size_t Size() const override;
  bool Read(Delegate* delegate) const override;

  const MemorySnapshot* MergeWithOtherSnapshot(
      const MemorySnapshot* other) const override {
    return nullptr;
  }

 private:
  const MemorySnapshot* snapshot_;
  RangeSet* ranges_;
  bool is_64_bit_;

  DISALLOW_COPY_AND_ASSIGN(StackSnapshotSanitized);
};

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_SANITIZED_STACK_SNAPSHOT_SANITIZED_H_
