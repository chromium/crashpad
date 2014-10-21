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

#ifndef CRASHPAD_MINIDUMP_TEST_MINIDUMP_WRITABLE_TEST_UTIL_H_
#define CRASHPAD_MINIDUMP_TEST_MINIDUMP_WRITABLE_TEST_UTIL_H_

#include <dbghelp.h>

#include <string>

#include "gtest/gtest.h"

namespace crashpad {
namespace test {

//! \brief Returns an untyped minidump object located within a minidump file’s
//!     contents, where the offset of the object is known.
//!
//! \param[in] file_contents The contents of the minidump file.
//! \param[in] rva The offset within the minidump file of the desired object.
//!
//! \return If \a rva is within the range of \a file_contents, returns a pointer
//!     into \a file_contents at offset \a rva. Otherwise, raises a gtest
//!     assertion failure and returns `nullptr`.
//!
//! Do not call this function. Use the typed version, MinidumpWritableAtRVA<>(),
//! or another type-specific function.
//!
//! \sa MinidumpWritableAtLocationDescriptorInternal()
const void* MinidumpWritableAtRVAInternal(const std::string& file_contents,
                                          RVA rva);

//! \brief Returns an untyped minidump object located within a minidump file’s
//!     contents, where the offset and size of the object are known.
//!
//! \param[in] file_contents The contents of the minidump file.
//! \param[in] location A MINIDUMP_LOCATION_DESCRIPTOR giving the offset within
//!     the minidump file of the desired object, as well as its size.
//! \param[in] expected_minimum_size The minimum size allowable for the object.
//!
//! \return If the size of \a location is at least as big as \a
//!     expected_minimum_size, and if \a location is within the range of \a
//!     file_contents, returns a pointer into \a file_contents at offset \a rva.
//!     Otherwise, raises a gtest assertion failure and returns `nullptr`.
//!
//! Do not call this function. Use the typed version,
//! MinidumpWritableAtLocationDescriptor<>(), or another type-specific function.
//!
//! \sa MinidumpWritableAtRVAInternal()
const void* MinidumpWritableAtLocationDescriptorInternal(
    const std::string& file_contents,
    const MINIDUMP_LOCATION_DESCRIPTOR& location,
    size_t expected_minimum_size);

//! \brief Returns a typed minidump object located within a minidump file’s
//!     contents, where the offset of the object is known.
//!
//! \param[in] file_contents The contents of the minidump file.
//! \param[in] rva The offset within the minidump file of the desired object.
//!
//! \return If \a rva is within the range of \a file_contents, returns a pointer
//!     into \a file_contents at offset \a rva. Otherwise, raises a gtest
//!     assertion failure and returns `nullptr`.
//!
//! \sa MinidumpWritableAtLocationDescriptor<>()
template <typename T>
const T* MinidumpWritableAtRVA(const std::string& file_contents, RVA rva) {
  return reinterpret_cast<const T*>(
      MinidumpWritableAtRVAInternal(file_contents, rva));
}

//! \brief Returns a typed minidump object located within a minidump file’s
//!     contents, where the offset and size of the object are known.
//!
//! \param[in] file_contents The contents of the minidump file.
//! \param[in] location A MINIDUMP_LOCATION_DESCRIPTOR giving the offset within
//!     the minidump file of the desired object, as well as its size.
//!
//! \return If the size of \a location is at least as big as the size of the
//!     requested object, and if \a location is within the range of \a
//!     file_contents, returns a pointer into \a file_contents at offset \a rva.
//!     Otherwise, raises a gtest assertion failure and returns `nullptr`.
//!
//! \sa MinidumpWritableAtRVA()
template <typename T>
const T* MinidumpWritableAtLocationDescriptor(
    const std::string& file_contents,
    const MINIDUMP_LOCATION_DESCRIPTOR& location) {
  return reinterpret_cast<const T*>(
      MinidumpWritableAtLocationDescriptorInternal(
          file_contents, location, sizeof(T)));
}

}  // namespace test
}  // namespace crashpad

#endif  // CRASHPAD_MINIDUMP_TEST_MINIDUMP_WRITABLE_TEST_UTIL_H_
