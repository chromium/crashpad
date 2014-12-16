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

#include "base/basictypes.h"

#include <stdint.h>

#include "client/simple_string_dictionary.h"

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

  void set_simple_annotations(SimpleStringDictionary* simple_annotations) {
    simple_annotations_ = simple_annotations;
  }

  static const uint32_t kSignature = 'CPad';

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
  uint32_t padding_0_;
  SimpleStringDictionary* simple_annotations_;  // weak

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

  DISALLOW_COPY_AND_ASSIGN(CrashpadInfo);
};

}  // namespace crashpad

#endif  // CRASHPAD_CLIENT_CRASHPAD_INFO_H_
