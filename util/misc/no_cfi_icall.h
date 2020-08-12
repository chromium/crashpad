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

#ifndef CRASHPAD_UTIL_MISC_NO_CFI_ICALL_H_
#define CRASHPAD_UTIL_MISC_NO_CFI_ICALL_H_

#include <type_traits>
#include <utility>

namespace crashpad {

//! \see NoCfiIcall<R(Args...)>
template <typename _>
class NoCfiIcall;

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
//! This class encapsulates cross-DSO function pointers to disable cfi-icall
//! precisely when executing these pointers.
template <typename R, typename... Args>
class NoCfiIcall<R(Args...)> {
 public:
  using CallType = R(Args...);

  //! \brief Constructs this object.
  //!
  //! \param function A pointer to the function to be called.
  explicit NoCfiIcall(CallType* function) : function_(function) {}

  //! \see NoCfiIcall
  template <typename PointerType,
            typename = std::enable_if_t<
                std::is_same<typename std::remove_cv<PointerType>::type,
                             void*>::value>>
  explicit NoCfiIcall(PointerType function)
      : function_(reinterpret_cast<CallType*>(function)) {}

  ~NoCfiIcall() = default;

  //! \brief Calls the function without sanitization by cfi-icall.
  __attribute__((no_sanitize("cfi-icall"))) R operator()(Args... args) const {
    return function_(std::forward<Args>(args)...);
  }

  //! \brief Returns `true` if not `nullptr`.
  operator bool() const { return function_ != nullptr; }

 private:
  NoCfiIcall(const NoCfiIcall&) = delete;
  void operator=(const NoCfiIcall&) = delete;

  CallType* function_;
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_MISC_NO_CFI_ICALL_H_
