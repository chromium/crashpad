// Copyright 2014 The Crashpad Authors. All rights reserved.
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

#include "util/mac/checked_mach_address_range.h"

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"

namespace crashpad {

CheckedMachAddressRange::CheckedMachAddressRange()
    : range_32_(0, 0), is_64_bit_(false), range_ok_(true) {
}

CheckedMachAddressRange::CheckedMachAddressRange(
    bool is_64_bit,
    mach_vm_address_t base,
    mach_vm_size_t size) {
  SetRange(is_64_bit, base, size);
}

void CheckedMachAddressRange::SetRange(bool is_64_bit,
                                       mach_vm_address_t base,
                                       mach_vm_size_t size) {
  is_64_bit_ = is_64_bit;
  if (is_64_bit_) {
    range_64_.SetRange(base, size);
    range_ok_ = true;
  } else {
    range_32_.SetRange(base, size);
    range_ok_ = base::IsValueInRangeForNumericType<uint32_t>(base) &&
                base::IsValueInRangeForNumericType<uint32_t>(size);
  }
}

mach_vm_address_t CheckedMachAddressRange::Base() const {
  return is_64_bit_ ? range_64_.base() : range_32_.base();
}

mach_vm_size_t CheckedMachAddressRange::Size() const {
  return is_64_bit_ ? range_64_.size() : range_32_.size();
}

mach_vm_address_t CheckedMachAddressRange::End() const {
  return is_64_bit_ ? range_64_.end() : range_32_.end();
}

bool CheckedMachAddressRange::IsValid() const {
  return range_ok_ && (is_64_bit_ ? range_64_.IsValid() : range_32_.IsValid());
}

bool CheckedMachAddressRange::ContainsValue(mach_vm_address_t value) const {
  DCHECK(range_ok_);

  if (is_64_bit_) {
    return range_64_.ContainsValue(value);
  }

  if (!base::IsValueInRangeForNumericType<uint32_t>(value)) {
    return false;
  }

  return range_32_.ContainsValue(value);
}

bool CheckedMachAddressRange::ContainsRange(
    const CheckedMachAddressRange& that) const {
  DCHECK_EQ(is_64_bit_, that.is_64_bit_);
  DCHECK(range_ok_);
  DCHECK(that.range_ok_);

  return is_64_bit_ ? range_64_.ContainsRange(that.range_64_)
                    : range_32_.ContainsRange(that.range_32_);
}

}  // namespace crashpad
