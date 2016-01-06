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

#ifndef CRASHPAD_CLIENT_CRASHPAD_INFO_H_
#define CRASHPAD_CLIENT_CRASHPAD_INFO_H_

#include <stdint.h>

#include "base/macros.h"
#include "build/build_config.h"
#include "client/simple_string_dictionary.h"
#include "util/misc/tri_state.h"

#if defined(OS_WIN)
#include <windows.h>
#endif  // OS_WIN

namespace crashpad {

//! \brief A structure that can be used by a Crashpad-enabled program to
//!     provide information to the Crashpad crash handler.
//!
//! It is possible for one CrashpadInfo structure to appear in each loaded code
//! module in a process, but from the perspective of the user of the client
//! interface, there is only one global CrashpadInfo structure, located in the
//! module that contains the client interface code.
struct CrashpadInfo {
 public:
  //! \brief Returns the global CrashpadInfo structure.
  static CrashpadInfo* GetCrashpadInfo();

  CrashpadInfo();

  //! \brief Sets the simple annotations dictionary.
  //!
  //! Simple annotations set on a CrashpadInfo structure are interpreted by
  //! Crashpad as module-level annotations.
  //!
  //! Annotations may exist in \a simple_annotations at the time that this
  //! method is called, or they may be added, removed, or modified in \a
  //! simple_annotations after this method is called.
  //!
  //! \param[in] simple_annotations A dictionary that maps string keys to string
  //!     values. The CrashpadInfo object does not take ownership of the
  //!     SimpleStringDictionary object. It is the caller’s responsibility to
  //!     ensure that this pointer remains valid while it is in effect for a
  //!     CrashpadInfo object.
  void set_simple_annotations(SimpleStringDictionary* simple_annotations) {
    simple_annotations_ = simple_annotations;
  }

  //! \brief Enables or disables Crashpad handler processing.
  //!
  //! When handling an exception, the Crashpad handler will scan all modules in
  //! a process. The first one that has a CrashpadInfo structure populated with
  //! a value other than #kUnset for this field will dictate whether the handler
  //! is functional or not. If all modules with a CrashpadInfo structure specify
  //! #kUnset, the handler will be enabled. If disabled, the Crashpad handler
  //! will still run and receive exceptions, but will not take any action on an
  //! exception on its own behalf, except for the action necessary to determine
  //! that it has been disabled.
  //!
  //! The Crashpad handler should not normally be disabled. More commonly, it
  //! is appropraite to disable crash report upload by calling
  //! Settings::SetUploadsEnabled().
  void set_crashpad_handler_behavior(TriState crashpad_handler_behavior) {
    crashpad_handler_behavior_ = crashpad_handler_behavior;
  }

  //! \brief Enables or disables Crashpad forwarding of exceptions to the
  //!     system’s crash reporter.
  //!
  //! When handling an exception, the Crashpad handler will scan all modules in
  //! a process. The first one that has a CrashpadInfo structure populated with
  //! a value other than #kUnset for this field will dictate whether the
  //! exception is forwarded to the system’s crash reporter. If all modules with
  //! a CrashpadInfo structure specify #kUnset, forwarding will be enabled.
  //! Unless disabled, forwarding may still occur if the Crashpad handler is
  //! disabled by SetCrashpadHandlerState(). Even when forwarding is enabled,
  //! the Crashpad handler may choose not to forward all exceptions to the
  //! system’s crash reporter in cases where it has reason to believe that the
  //! system’s crash reporter would not normally have handled the exception in
  //! Crashpad’s absence.
  void set_system_crash_reporter_forwarding(
      TriState system_crash_reporter_forwarding) {
    system_crash_reporter_forwarding_ = system_crash_reporter_forwarding;
  }

  enum : uint32_t {
    kSignature = 'CPad',
  };

 private:
  // The compiler won’t necessarily see anyone using these fields, but it
  // shouldn’t warn about that. These fields aren’t intended for use by the
  // process they’re found in, they’re supposed to be read by the crash
  // reporting process.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-private-field"
#endif

  // Fields present in version 1:
  uint32_t signature_;  // kSignature
  uint32_t size_;  // The size of the entire CrashpadInfo structure.
  uint32_t version_;  // kVersion
  TriState crashpad_handler_behavior_;
  TriState system_crash_reporter_forwarding_;
  uint16_t padding_0_;
  SimpleStringDictionary* simple_annotations_;  // weak

#if !defined(NDEBUG) && defined(OS_WIN)
  uint32_t invalid_read_detection_;
#endif

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

  DISALLOW_COPY_AND_ASSIGN(CrashpadInfo);
};

}  // namespace crashpad

#endif  // CRASHPAD_CLIENT_CRASHPAD_INFO_H_
