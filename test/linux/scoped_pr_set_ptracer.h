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

#ifndef CRASHPAD_TEST_LINUX_SCOPED_PR_SET_PTRACER_H_
#define CRASHPAD_TEST_LINUX_SCOPED_PR_SET_PTRACER_H_

#include <sys/types.h>

#include "base/macros.h"

namespace crashpad {
namespace test {

class ScopedPrSetPtracer {
 public:
  //! \brief Uses `PR_SET_PTRACER` to set \a pid as the caller's ptracer or
  //!     expects `EINVAL`.
  //!
  //! `PR_SET_PTRACER` is only supported if the Yama Linux security module (LSM)
  //! is enabled. Otherwise, `prctl(PR_SET_PTRACER, ...)` fails with `EINVAL`.
  //! See linux-4.9.20/security/yama/yama_lsm.c yama_task_prctl() and
  //! linux-4.9.20/kernel/sys.c [sys_]prctl().
  explicit ScopedPrSetPtracer(pid_t pid);

  ~ScopedPrSetPtracer();

 private:
  bool success_;

  DISALLOW_COPY_AND_ASSIGN(ScopedPrSetPtracer);
};

}  // namespace test
}  // namespace crashpad

#endif  // CRASHPAD_TEST_LINUX_SCOPED_PR_SET_PTRACER_H_
