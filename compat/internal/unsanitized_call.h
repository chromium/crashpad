// Copyright 2020 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_COMPAT_INTERNAL_UNSANITIZED_CALL_H_
#define CRASHPAD_COMPAT_INTERNAL_UNSANITIZED_CALL_H_

#include <utility>

namespace crashpad {

//! \brief Disables cfi-icall for calls made through a function pointer.
//!
//! Clang provides several Control-Flow-Integrity (CFI) sanitizers, among them,
//! cfi-icall, which attempts to verify that the dynamic type of a function
//! matches the static type of the function pointer used to call it.
//!
//! https://clang.llvm.org/docs/ControlFlowIntegrity.html#indirect-function-call-checking
//!
//! However, cfi-icall does not have enough information to check indirect calls
//! to functions in other modules, such as through the pointers returned by
//! `dlsym()`. In these cases, CFI aborts the program upon executing the
//! indirect call.
//!
//! This class encapsulates cross-dso function pointers to disable cfi-icall
//! precisely when executing these pointers.
template <typename _>
class UnsanitizedCall;
template <typename R, typename... Args>
class UnsanitizedCall<R(Args...)> {
 public:
  using CallType = R(Args...);

  //! \brief Constructs this object.
  //!
  //! \param function A pointer to the function to be called.
  explicit UnsanitizedCall(CallType* function) : function_(function) {}

  ~UnsanitizedCall() = default;

  //! \brief Calls the function without sanitization by cfi-icall.
  __attribute__((no_sanitize("cfi-icall"))) R operator()(Args... args) const {
    return function_(std::forward<Args>(args)...);
  }

  //! \brief Returns `true` if not `nullptr`.
  operator bool() const { return function_ != nullptr; }

 private:
  UnsanitizedCall(const UnsanitizedCall&) = delete;
  void operator=(const UnsanitizedCall&) = delete;

  CallType* function_;
};

}  // namespace crashpad

#endif  // CRASHPAD_COMPAT_INTERNAL_UNSANITIZED_CALL_H_
