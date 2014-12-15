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

#ifndef CRASHPAD_UTIL_STDLIB_CXX_H_
#define CRASHPAD_UTIL_STDLIB_CXX_H_

#include "build/build_config.h"

#if defined(COMPILER_MSVC)

#define CXX_LIBRARY_VERSION 2011
#define CXX_LIBRARY_HAS_CONSTEXPR 0

#else  // !COMPILER_MSVC

// <ciso646> doesn’t do very much, and under libc++, it will cause the
// _LIBCPP_VERSION macro to be defined properly. Under libstdc++, it doesn’t
// cause __GLIBCXX__ to be defined, but if _LIBCPP_VERSION isn’t defined after
// #including <ciso646>, assume libstdc++ and #include libstdc++’s internal
// header that defines __GLIBCXX__.

#include <ciso646>

#if !defined(_LIBCPP_VERSION)
#if defined(__has_include)
#if __has_include(<bits/c++config.h>)
#include <bits/c++config.h>
#endif
#else
#include <bits/c++config.h>
#endif
#endif

// libstdc++ does not identify its version directly, but identifies the GCC
// package it is a part of via the __GLIBCXX__ macro, which is based on the date
// of the GCC release. libstdc++ had sufficient C++11 support as of GCC 4.6.0,
// __GLIBCXX__ 20110325, but maintenance releases in the 4.4 an 4.5 series
// followed this date, so check for those versions by their date stamps.
// http://gcc.gnu.org/onlinedocs/libstdc++/manual/abi.html#abi.versioning
//
// libc++, identified by _LIBCPP_VERSION, always supports C++11.
#if __cplusplus >= 201103l &&                                                 \
    ((defined(__GLIBCXX__) &&                                                 \
      __GLIBCXX__ >= 20110325ul &&  /* GCC >= 4.6.0 */                        \
      __GLIBCXX__ != 20110416ul &&  /* GCC 4.4.6 */                           \
      __GLIBCXX__ != 20120313ul &&  /* GCC 4.4.7 */                           \
      __GLIBCXX__ != 20110428ul &&  /* GCC 4.5.3 */                           \
      __GLIBCXX__ != 20120702ul) ||  /* GCC 4.5.4 */                          \
     (defined(_LIBCPP_VERSION)))
#define CXX_LIBRARY_VERSION 2011
#define CXX_LIBRARY_HAS_CONSTEXPR 1
#else
#define CXX_LIBRARY_VERSION 2003
#define CXX_LIBRARY_HAS_CONSTEXPR 0
#endif

#endif  // COMPILER_MSVC

#endif  // CRASHPAD_UTIL_STDLIB_CXX_H_
