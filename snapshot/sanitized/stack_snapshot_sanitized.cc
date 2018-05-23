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

#include "snapshot/sanitized/stack_snapshot_sanitized.h"

namespace crashpad {
namespace internal {

namespace {

class StackSanitizer : public MemorySnapshot::Delegate {
 public:
  StackSanitizer(MemorySnapshot::Delegate* delegate,
                 RangeSet* ranges,
                 bool is_64_bit)
      : delegate_(delegate), ranges_(ranges), is_64_bit_(is_64_bit) {}
  ~StackSanitizer() = default;

  bool MemorySnapshotDelegateRead(void* data, size_t size) override {
    is_64_bit_ ? Sanitize<uint64_t>(data, size)
               : Sanitize<uint32_t>(data, size);
    return delegate_->MemorySnapshotDelegateRead(data, size);
  }

 private:
  template <typename Pointer>
  void Sanitize(void* data, size_t size) {
    size_t word_count = size / sizeof(Pointer);
    Pointer* words = static_cast<Pointer*>(data);
    for (size_t index = 0; index < word_count; ++index) {
      if (!ranges_->Contains(words[index])) {
        words[index] = static_cast<Pointer>(StackSnapshotSanitized::kDefaced);
      }
    }
  }

  MemorySnapshot::Delegate* delegate_;
  RangeSet* ranges_;
  bool is_64_bit_;

  DISALLOW_COPY_AND_ASSIGN(StackSanitizer);
};

}  // namespace

StackSnapshotSanitized::StackSnapshotSanitized(const MemorySnapshot* snapshot,
                                               RangeSet* ranges)
    : snapshot_(snapshot), ranges_(ranges) {}

StackSnapshotSanitized::~StackSnapshotSanitized() = default;

uint64_t StackSnapshotSanitized::Address() const {
  return snapshot_->Address();
}

size_t StackSnapshotSanitized::Size() const {
  return snapshot_->Size();
}

bool StackSnapshotSanitized::Read(Delegate* delegate) const {
  StackSanitizer sanitizer(delegate, ranges_, true);  // TODO
  return snapshot_->Read(&sanitizer);
}

}  // namespace internal
}  // namespace crashpad
