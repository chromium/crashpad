// Copyright 2020 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_SNAPSHOT_SANITIZED_POINTER_SANITIZER_H_
#define CRASHPAD_SNAPSHOT_SANITIZED_POINTER_SANITIZER_H_

#include <stdint.h>

#include "snapshot/memory_snapshot.h"
#include "util/misc/range_set.h"

namespace crashpad {
namespace internal {

//! \brief A MemorySnapshot which wraps and filters sensitive information from
//!     another MemorySnapshot.
//!
//! This class redacts all data from the wrapped MemorySnapshot unless:
//! 1. The data is pointer aligned and points into a whitelisted address range.
//! 2. The data is pointer aligned and is a small integer.
class PointerSanitizer final {
 public:
  //! \brief Pointer-aligned data smaller than this value is not redacted.
  static constexpr uint64_t kSmallWordMax = 4096;

  //! \brief Redacted data is replaced with this value, casted to the
  //!     appropriate size for a pointer in the target process.
  static constexpr uint64_t kDefaced = 0x0defaced0defaced;

  //! \brief Constructs this object.
  //!
  //! \param[in] ranges A set of whitelisted address ranges.
  PointerSanitizer(RangeSet* ranges);

  ~PointerSanitizer();

  template <typename Pointer>
  Pointer SanitizeValue(Pointer value) {
    const Pointer defaced = static_cast<Pointer>(kDefaced);

    if (value > kSmallWordMax && !ranges_->Contains(value)) {
      return defaced;
    }
    return value;
  }

  template <typename Pointer>
  void Sanitize(Pointer* data, size_t word_count) {
    for (size_t index = 0; index < word_count; ++index) {
      data[index] = SanitizeValue(data[index]);
    }
  }

  template <typename Pointer>
  void SanitizeMemory(void* data, VMAddress base_address, size_t size) {
    const Pointer defaced = static_cast<Pointer>(kDefaced);

    // Sanitize up to a word-aligned address.
    const size_t aligned_offset =
        ((base_address + sizeof(Pointer) - 1) & ~(sizeof(Pointer) - 1)) -
        base_address;
    memcpy(data, &defaced, aligned_offset);

    // Sanitize words that aren't small and don't look like pointers.
    size_t word_count = (size - aligned_offset) / sizeof(Pointer);
    auto words =
        reinterpret_cast<Pointer*>(static_cast<char*>(data) + aligned_offset);
    Sanitize(words, word_count);

    // Sanitize trailing bytes beyond the word-sized items.
    const size_t sanitized_bytes =
        aligned_offset + word_count * sizeof(Pointer);
    memcpy(static_cast<char*>(data) + sanitized_bytes,
           &defaced,
           size - sanitized_bytes);
  }

 private:
  RangeSet* ranges_;

  DISALLOW_COPY_AND_ASSIGN(PointerSanitizer);
};

}  // namespace internal
}  // namespace crashpad

#endif  // CRASHPAD_SNAPSHOT_SANITIZED_POINTER_SANITIZER_H_
