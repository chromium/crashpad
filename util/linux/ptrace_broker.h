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

#ifndef CRASHPAD_UTIL_LINUX_PTRACE_BROKER_H_
#define CRASHPAD_UTIL_LINUX_PTRACE_BROKER_H_

#include <stdint.h>
#include <sys/types.h>

#include "base/macros.h"
#include "util/linux/ptrace_connection.h"
#include "util/linux/ptracer.h"
#include "util/linux/thread_info.h"

namespace crashpad {

//! \brief Implements the PtraceConnection interface over a socket.
//!
//! This class is used in a process with `ptrace` capabilities for the target
//! process and may run in a compromised context.
class PtraceBroker {
 public:
#pragma pack(push, 1)
  struct Request {
    enum class Type : int32_t {
      kAttach,
      kIs64Bit,
      kGetThreadInfo,
      kExit
    } type;

    pid_t tid;
  };

  struct GetThreadInfoResponse {
    ThreadInfo info;
    bool success;
  };
#pragma pack(pop)

  PtraceBroker(int sock, bool is_64_bit);
  ~PtraceBroker();

  //! \brief Begin serving requests on the configured socket.
  //!
  //! This method returns when a PtraceBrokerRequest::Type::kExit is received or
  //! an error is encountered on the socket.
  //!
  //! \return `true` if Run() exited normally, otherwise `false` with an error
  //!     logged.
  bool Run();

 private:
  int sock_;
  Ptracer ptracer_;

  DISALLOW_COPY_AND_ASSIGN(PtraceBroker);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_LINUX_PTRACE_BROKER_H_
