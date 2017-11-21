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

//! \brief Implements a PtraceConnection over a socket.
//!
//! This class is the server half of the connection and should by run in a
//! process with `ptrace` capabilities for the target process and may run in a
//! compromised context.
class PtraceBroker {
 public:
#pragma pack(push, 1)
  //! \brief A boolean status suitable for communication between processes.
  enum class Bool : char {
    kFalse,
    kTrue
  };

  //! \brief A request sent to a PtraceBroker from a PtraceClient.
  struct Request {
    //! \brief The type of request to execute.
    enum class Type : int32_t {
      //! \brief `ptrace`-attach the specified thread ID. Responds with
      //!     Bool::kTrue on success, otherwise Bool::kFalse.
      kAttach,

      //! \brief Determines the bitness of the target process.
      kIs64Bit,

      //! \brief Gets a ThreadInfo for the specified thread ID.
      kGetThreadInfo,

      //! \brief Causes the broker to return from Run().
      kExit
    } type;

    pid_t tid;
  };

  //! \brief The response sent for a Request::Type::kGetThreadInfo.
  struct GetThreadInfoResponse {
    //! \brief Information about the specified thread. Only valid if status is
    //!     Bool::kTrue.
    ThreadInfo info;

    //! \brief Specifies the success or failure of this call.
    Bool success;
  };
#pragma pack(pop)

  PtraceBroker(int sock, bool is_64_bit);
  ~PtraceBroker();

  //! \brief Begin serving requests on the configured socket.
  //!
  //! This method returns when a PtraceBrokerRequest with Type::kExit is
  //! received or an error is encountered on the socket.
  //!
  //! \return `true` if Run() exited due to an exit request. Otherwise `false`.
  bool Run();

 private:
  int sock_;
  Ptracer ptracer_;

  DISALLOW_COPY_AND_ASSIGN(PtraceBroker);
};

}  // namespace crashpad

#endif  // CRASHPAD_UTIL_LINUX_PTRACE_BROKER_H_
