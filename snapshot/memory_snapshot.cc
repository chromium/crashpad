// Copyright 2016 The Crashpad Authors. All rights reserved.
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

#include "snapshot/memory_snapshot.h"

#include <algorithm>

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "util/numeric/checked_range.h"

namespace crashpad {
namespace {

bool DetermineMergedRangeImpl(bool log,
                          const MemorySnapshot& a,
                          const MemorySnapshot& b,
                          CheckedRange<uint64_t, size_t>* merged) {
  CheckedRange<uint64_t, size_t> this_range(a.Address(), a.Size());
  if (!this_range.IsValid()) {
    if (log) {
      LOG(ERROR) << base::StringPrintf("invalid range at 0x%llx, size %" PRIuS,
                                       this_range.base(),
                                       this_range.size());
    }
    return false;
  }

  CheckedRange<uint64_t, size_t> other_range(b.Address(), b.Size());
  if (!other_range.IsValid()) {
    if (log) {
      LOG(ERROR) << base::StringPrintf("invalid range at 0x%llx, size %" PRIuS,
                                       other_range.base(),
                                       other_range.size());
    }
    return false;
  }

  if (this_range.size() == 0 && other_range.size() == 0) {
    if (log) {
      LOG(ERROR) << base::StringPrintf("two empty ranges at 0x%llx and 0x%llx",
                                       this_range.base(),
                                       other_range.base());
    }
    return false;
  }

  if (this_range.size() == 0) {
    if (merged)
      *merged = other_range;
    return true;
  }

  if (other_range.size() == 0) {
    if (merged)
      *merged = this_range;
    return true;
  }

  if (!this_range.OverlapsRange(other_range) &&
      this_range.end() != other_range.base() &&
      other_range.end() != this_range.base()) {
    if (log) {
      LOG(ERROR) << base::StringPrintf(
          "ranges not overlapping or abutting: (0x%llx, size %" PRIuS
          ") and "
          "(0x%llx, size %" PRIuS ")",
          this_range.base(),
          this_range.size(),
          other_range.base(),
          other_range.size());
    }
    return false;
  }

  if (merged) {
    uint64_t base = std::min(this_range.base(), other_range.base());
    uint64_t end = std::max(this_range.end(), other_range.end());
    size_t size = static_cast<size_t>(end - base);
    merged->SetRange(base, size);
  }
  return true;
}

}  // namespace

bool LoggingDetermineMergedRange(const MemorySnapshot& a,
                                 const MemorySnapshot& b,
                                 CheckedRange<uint64_t, size_t>* merged) {
  return DetermineMergedRangeImpl(true, a, b, merged);
}

bool DetermineMergedRange(const MemorySnapshot& a,
                          const MemorySnapshot& b,
                          CheckedRange<uint64_t, size_t>* merged) {
  return DetermineMergedRangeImpl(false, a, b, merged);
}

}  // namespace crashpad
