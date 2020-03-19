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

#include "snapshot/sanitized/memory_snapshot_sanitized.h"

#include "snapshot/sanitized/pointer_sanitizer.h"

#include <string.h>

namespace crashpad {
namespace internal {

namespace {

class MemorySanitizer : public MemorySnapshot::Delegate {
 public:
  MemorySanitizer(MemorySnapshot::Delegate* delegate,
                  PointerSanitizer* sanitizer,
                  VMAddress address,
                  bool is_64_bit)
      : delegate_(delegate),
        sanitizer_(sanitizer),
        address_(address),
        is_64_bit_(is_64_bit) {}

  ~MemorySanitizer() = default;

  bool MemorySnapshotDelegateRead(void* data, size_t size) override {
    if (is_64_bit_) {
      sanitizer_->SanitizeMemory<uint64_t>(data, address_, size);
    } else {
      sanitizer_->SanitizeMemory<uint32_t>(data, address_, size);
    }
    return delegate_->MemorySnapshotDelegateRead(data, size);
  }

 private:

  MemorySnapshot::Delegate* delegate_;
  PointerSanitizer* sanitizer_;
  VMAddress address_;
  bool is_64_bit_;

  DISALLOW_COPY_AND_ASSIGN(MemorySanitizer);
};

}  // namespace

MemorySnapshotSanitized::MemorySnapshotSanitized(const MemorySnapshot* snapshot,
                                                 PointerSanitizer* sanitizer,
                                                 bool is_64_bit)
    : sanitizer_(sanitizer), snapshot_(snapshot), is_64_bit_(is_64_bit) {}

MemorySnapshotSanitized::~MemorySnapshotSanitized() = default;

uint64_t MemorySnapshotSanitized::Address() const {
  return snapshot_->Address();
}

size_t MemorySnapshotSanitized::Size() const {
  return snapshot_->Size();
}

bool MemorySnapshotSanitized::Read(Delegate* delegate) const {
  MemorySanitizer sanitizer(delegate, sanitizer_, Address(), is_64_bit_);
  return snapshot_->Read(&sanitizer);
}

}  // namespace internal
}  // namespace crashpad
