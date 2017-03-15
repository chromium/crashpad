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

#ifndef CRASHPAD_MINIDUMP_MINIDUMP_USER_EXTENSION_STREAM_H_
#define CRASHPAD_MINIDUMP_MINIDUMP_USER_EXTENSION_STREAM_H_

#include <stdint.h>
#include <sys/types.h>
#include <time.h>

#include <string>

#include "base/macros.h"
#include "base/strings/string16.h"

namespace crashpad {

//! \brief TODO(siggi): document me. DO NOT SUBMIT.
class MinidumpUserExtensionStream final {
 public:
  MinidumpUserExtensionStream();
  ~MinidumpUserExtensionStream();

  uint32_t stream_type() const { return stream_type_; }

 private:
  uint32_t stream_type_;

  DISALLOW_COPY_AND_ASSIGN(MinidumpUserExtensionStream);
};

}  // namespace crashpad

#endif  // CRASHPAD_MINIDUMP_MINIDUMP_USER_EXTENSION_STREAM_H_
