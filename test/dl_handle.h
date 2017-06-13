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

#ifndef CRASHPAD_TEST_DL_HANDLE_H_
#define CRASHPAD_TEST_DL_HANDLE_H_

#include "base/macros.h"
#include "build/build_config.h"

#if defined(OS_POSIX)
#include <dlfcn.h>
#elif defined(OS_WIN)
#include <windows.h>
#endif

namespace crashpad {
namespace test {

#if defined(OS_POSIX) || DOXYGEN
//! \brief The native type used to represent a handle to a module loaded in the
//!     current process.
using DlHandle = void*;
#elif defined(OS_WIN)
using DlHandle = HMODULE;
#endif  // OS_POSIX

//! \return The value of the symbol named by \a symbol_name in the module
//!     identified by \a dl_handle, or `nullptr` on failure.
void* LookUpSymbol(DlHandle dl_handle, const char* symbol_name);

//! \brief Maintains ownership of a #DlHandle object, releasing it as
//!     appropriate on destruction.
class ScopedDlHandle {
 public:
  explicit ScopedDlHandle(DlHandle dl_handle);
  ~ScopedDlHandle();

  //! \return `true` if this object manages a valid #DlHandle.
  bool valid() const { return dl_handle_ != nullptr; }

  //! \return The value of the symbol named by \a symbol_name, or `nullptr` on
  //!     failure.
  template <typename T>
  T LookUpSymbol(const char* symbol_name) {
    return reinterpret_cast<T>(test::LookUpSymbol(dl_handle_, symbol_name));
  }

 private:
  DlHandle dl_handle_;

  DISALLOW_COPY_AND_ASSIGN(ScopedDlHandle);
};

}  // namespace test
}  // namespace crashpad

#endif  // CRASHPAD_TEST_DL_HANDLE_H_
