// Copyright 2015 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_UTIL_STDLIB_MOVE_H_
#define CRASHPAD_UTIL_STDLIB_MOVE_H_

#include "util/stdlib/cxx.h"

namespace crashpad {

#if CXX_LIBRARY_VERSION >= 2011
//! \brief A typedef for std::remove_reference until C++11 library support is
//     always available.
template <typename T>
using remove_reference = std::remove_reference<T>;
#else
//! \brief A replacement for std::remove_reference until C++11 library support
//     is always available.
template <class T>
struct remove_reference { using type = T; };
template <class T>
struct remove_reference<T&> { using type = T; };
#endif  // CXX_LIBRARY_VERSION

#if CXX_LIBRARY_VERSION >= 2011
//! \brief A wrapper around std::move() until C++11 library support is
//     always available.
template <typename T>
typename std::remove_reference<T>::type&& move(T&& t) {
  return std::move(t);
}
#else
//! \brief A replacement for std::move() until C++11 library support is
//     always available.
template <typename T>
typename remove_reference<T>::type&& move(T&& t) {
  return static_cast<typename remove_reference<T>::type&&>(t);
}
#endif  // CXX_LIBRARY_VERSION

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_STDLIB_MOVE_H_
