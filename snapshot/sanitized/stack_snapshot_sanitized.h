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

#include "snapshot/memory_snapshot.h"
#include "util/misc/range_set.h"

namespace crashpad {
namespace internal {

class StackSnapshotSanitized final : public MemorySnapshot {
 public:
  static constexpr uint64_t kDefaced = 0xdefaceddefaced;

  StackSnapshotSanitized(const MemorySnapshot* stack, RangeSet* ranges_);
  ~StackSnapshotSanitized() override;

  // MemorySnapshot:

  uint64_t Address() const override;
  size_t Size() const override;
  bool Read(Delegate* delegate) const override;

  const MemorySnapshot* MergeWithOtherSnapshot(
      const MemorySnapshot* other) const override {
    return nullptr;  // TODO
  }

 private:
  const MemorySnapshot* snapshot_;
  RangeSet* ranges_;

  DISALLOW_COPY_AND_ASSIGN(StackSnapshotSanitized);
};

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_SANITIZED_STACK_SNAPSHOT_SANITIZED_H_
