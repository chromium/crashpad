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

#ifndef CRASHPAD_UTIL_LINUX_PTRACER_H_
#define CRASHPAD_UTIL_LINUX_PTRACER_H_

#include <sys/types.h>

#include "base/macros.h"
#include "util/linux/address_types.h"
#include "util/linux/ptrace_connection.h"

namespace crashpad {

class Ptracer {
 public:
  Ptracer(bool is_64_bit);
  Ptracer();
  ~Ptracer();

  bool Initialize(pid_t pid);
  bool GetThreadInfo(pid_t tid, ThreadInfo* info);

 private:
  bool is_64_bit_;

  DISALLOW_COPY_AND_ASSIGN(Ptracer);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_LINUX_PTRACER_H_
