// Copyright 2017 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_UTIL_MISC_FROM_POINTER_CAST_H_
#define CRASHPAD_UTIL_MISC_FROM_POINTER_CAST_H_

#include <stdint.h>

#include <cstddef>
#include <limits>
#include <type_traits>

namespace crashpad {

#if DOXYGEN

//! \brief Casts from a pointer type to an integer.
//!
//! Compared to `reinterpret_cast<>()`, FromPointerCast<>() defines whether a
//! pointer type is sign-extended or zero-extended. Casts to signed integral
//! types are sign-extended. Casts to unsigned integral types are zero-extended.
template <typename To, typename From>
FromPointerCast(From from) {
  return reinterpret_cast<To>(from);
}

#else  // DOXYGEN

// Cast std::nullptr_t to any pointer type.
template <typename To, typename From>
typename std::enable_if<
    std::is_same<typename std::remove_cv<From>::type, std::nullptr_t>::value &&
        std::is_pointer<To>::value,
    To>::type
FromPointerCast(From from, To* = nullptr) {
  return static_cast<To>(from);
}

// Cast std::nullptr_t to any integral type.
template <typename To, typename From>
typename std::enable_if<
    std::is_same<typename std::remove_cv<From>::type, std::nullptr_t>::value &&
        std::is_integral<To>::value,
    To>::type
FromPointerCast(From from, To* = nullptr) {
  return reinterpret_cast<To>(from);
}

// Cast to any other pointer type.
template <typename To, typename From>
typename std::enable_if<std::is_pointer<From>::value &&
                            std::is_pointer<To>::value,
                        To>::type
FromPointerCast(From from, To* = nullptr) {
  return reinterpret_cast<To>(from);
}

// Cast to a signed integral type, sign-extend.
template <typename To, typename From>
typename std::enable_if<std::is_pointer<From>::value &&
                            std::is_integral<To>::value &&
                            std::numeric_limits<To>::is_signed,
                        To>::type
FromPointerCast(From from, To* = nullptr) {
  return static_cast<To>(reinterpret_cast<intptr_t>(from));
}

// Cast to an unsigned integral type, zero-extend.
template <typename To, typename From>
typename std::enable_if<std::is_pointer<From>::value &&
                            std::is_integral<To>::value &&
                            !std::numeric_limits<To>::is_signed,
                        To>::type
FromPointerCast(From from, To* = nullptr) {
  return static_cast<To>(reinterpret_cast<uintptr_t>(from));
}

#endif  // DOXYGEN

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_MISC_FROM_POINTER_CAST_H_
