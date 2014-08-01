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

#ifndef CRASHPAD_UTIL_MISC_UUID_H_
#define CRASHPAD_UTIL_MISC_UUID_H_

#include <stdint.h>

#include <string>

namespace crashpad {

//! \brief A universally unique identifier (%UUID).
//!
//! An alternate term for %UUID is “globally unique identifier” (GUID), used
//! primarily by Microsoft.
//!
//! A %UUID is a unique 128-bit number specified by RFC 4122.
//!
//! This is a standard-layout structure, and it is acceptable to use `memcpy()`
//! to set its value.
struct UUID {
  //! \brief Initializes the %UUID to zero.
  UUID();

  //! \brief Initializes the %UUID from a sequence of bytes.
  //!
  //! \param[in] bytes A buffer of exactly 16 bytes that will be assigned to the
  //!     %UUID.
  explicit UUID(const uint8_t* bytes);

  //! \brief Formats the %UUID per RFC 4122 §3.
  //!
  //! \return A string of the form `"00112233-4455-6677-8899-aabbccddeeff"`.
  std::string ToString() const;

  uint8_t data[16];
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_MISC_UUID_H_
