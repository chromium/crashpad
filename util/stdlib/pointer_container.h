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

#ifndef CRASHPAD_UTIL_STDLIB_POINTER_CONTAINER_H_
#define CRASHPAD_UTIL_STDLIB_POINTER_CONTAINER_H_

#include <vector>

#include "base/macros.h"

namespace crashpad {

//! \brief Allows a `std::vector` to “own” pointer elements stored in it.
//!
//! When the vector is destroyed, `delete` will be called on its pointer
//! elements.
//!
//! \note No attempt is made to `delete` elements that are removed from the
//!     vector by other means, such as replacement or `clear()`.
template <typename T>
class PointerVector : public std::vector<T*> {
 public:
  PointerVector() : std::vector<T*>() {}

  ~PointerVector() {
    for (T* item : *this)
      delete item;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PointerVector);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_STDLIB_POINTER_CONTAINER_H_
