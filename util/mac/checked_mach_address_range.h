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

#ifndef CRASHPAD_UTIL_MAC_CHECKED_MACH_ADDRESS_RANGE_H_
#define CRASHPAD_UTIL_MAC_CHECKED_MACH_ADDRESS_RANGE_H_

#include <mach/mach.h>

#include "util/numeric/checked_range.h"

namespace crashpad {

//! \brief Ensures that a range, composed of a base and a size, does not
//!     overflow the pointer type of the process it describes a range in.
//!
//! This class checks bases of type `mach_vm_address_t` and sizes of type
//! `mach_vm_address_t` against a process whose pointer type is either 32 or 64
//! bits wide.
//!
//! Aside from varying the overall range on the basis of a process’ pointer type
//! width, this class functions very similarly to CheckedRange.
class CheckedMachAddressRange {
 public:
  //! \brief Initializes a default range.
  //!
  //! The default range has base 0, size 0, and appears to be from a 32-bit
  //! process.
  CheckedMachAddressRange();

  //! \brief Initializes a range.
  //!
  //! See SetRange().
  CheckedMachAddressRange(bool is_64_bit,
                          mach_vm_address_t base,
                          mach_vm_size_t size);

  //! \brief Sets a range’s fields.
  //!
  //! \param[in] is_64_bit `true` if \a base and \a size refer to addresses in a
  //!     64-bit process; `false` if they refer to addresses in a 32-bit
  //!     process.
  //! \param[in] base The range’s base address.
  //! \param[in] size The range’s size.
  void SetRange(bool is_64_bit, mach_vm_address_t base, mach_vm_size_t size);

  //! \brief The range’s base address.
  mach_vm_address_t Base() const;

  //! \brief The range’s size.
  mach_vm_size_t Size() const;

  //! \brief The range’s end address (its base address plus its size).
  mach_vm_address_t End() const;

  //! \brief Returns the validity of the address range.
  //!
  //! \return `true` if the address range is valid, `false` otherwise.
  //!
  //! An address range is valid if its size can be converted to the address
  //! range’s data type without data loss, and if its end (base plus size) can
  //! be computed without overflowing its data type.
  bool IsValid() const;

  //! \brief Returns whether the address range contains another address.
  //!
  //! \param[in] value The (possibly) contained address.
  //!
  //! \return `true` if the address range contains \a value, `false` otherwise.
  //!
  //! An address range contains a value if the value is greater than or equal to
  //! its base address, and less than its end address (base address plus size).
  //!
  //! This method must only be called if IsValid() would return `true`.
  bool ContainsValue(const mach_vm_address_t value) const;

  //! \brief Returns whether the address range contains another address range.
  //!
  //! \param[in] that The (possibly) contained address range.
  //!
  //! \return `true` if `this` address range, the containing address range,
  //!     contains \a that, the contained address range. `false` otherwise.
  //!
  //! An address range contains another address range when the contained address
  //! range’s base is greater than or equal to the containing address range’s
  //! base, and the contained address range’s end is less than or equal to the
  //! containing address range’s end.
  //!
  //! This method should only be called on two CheckedMachAddressRange objects
  //! representing address ranges in the same process.
  //!
  //! This method must only be called if IsValid() would return `true` for both
  //! CheckedMachAddressRange objects involved.
  bool ContainsRange(const CheckedMachAddressRange& that) const;

 private:
  // The field of the union that is expressed is determined by is_64_bit_.
  union {
    CheckedRange<uint32_t> range_32_;
    CheckedRange<uint64_t> range_64_;
  };

  // Determines which field of the union is expressed.
  bool is_64_bit_;

  // Whether the base and size were valid for their data type when set. This is
  // always true when is_64_bit_ is true because the underlying data types are
  // 64 bits wide and there is no possibility for range and size to overflow.
  // When is_64_bit_ is false, range_ok_ will be false if SetRange() was passed
  // a base or size that overflowed the underlying 32-bit data type. This field
  // is necessary because the interface exposes mach_vm_address_t and
  // mach_vm_size_t uniformly, but these types are too wide for the underlying
  // pointer and size types in 32-bit processes.
  bool range_ok_;

  DISALLOW_COPY_AND_ASSIGN(CheckedMachAddressRange);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_MAC_CHECKED_MACH_ADDRESS_RANGE_H_
